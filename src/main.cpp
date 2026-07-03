#include <Arduino.h>
#include "camera_handler.h"
#include "network_handler.h"
#include "config.h"

// 异步拍照任务与重试状态机控制变量
static volatile bool s_need_take_photo = false;
static bool s_in_retry_mode = false;
static int s_retry_count = 0;
static unsigned long s_last_capture_attempt_ms = 0;

static unsigned long s_last_photo_time_ms = 0;      // 上一次尝试或成功拍照的时间
static bool s_use_flash_next_time = false;          // 下一次常规拍摄是否需要开启闪光灯

const unsigned long RETRY_INTERVAL_MS = 10000UL; // 10秒重试间隔
const int MAX_RETRY_ATTEMPTS = 3;               // 最多重试3次

// 统一触发拍照与重载状态的辅助函数
void trigger_photo_capture() {
    s_need_take_photo = true;
    s_in_retry_mode = false;
    s_retry_count = 0;
}

// ============================================================
//  MQTT 命令接收回调
// ============================================================
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("[MQTT RX] Topic: %s\n", topic);
    if (strcmp(topic, MQTT_CMD_TOPIC) == 0) {
        char payload_str[32];
        unsigned int len = length < 31 ? length : 31;
        memcpy(payload_str, payload, len);
        payload_str[len] = '\0';
        
        Serial.printf("[MQTT RX] Payload: %s\n", payload_str);
        // 比对 Payload 是否与自身站点名称一致
        if (strcmp(payload_str, STATION_NAME) == 0) {
            trigger_photo_capture(); // 匹配成功，异步触发拍照并重置重试状态
        }
    }
}

// ============================================================
//  拍摄并发送照片的业务逻辑
// ============================================================
bool process_photo_job(bool use_flash) {
    if (use_flash) {
        Serial.println("[Photo] Enabling flash for capture...");
        digitalWrite(FLASH_GPIO_NUM, HIGH);
        delay(FLASH_WARMUP_MS); // 等待 AEC/AGC 收敛稳定，时长由 config.h 中 FLASH_WARMUP_MS 控制
    } else {
        digitalWrite(FLASH_GPIO_NUM, LOW);
    }

    camera_fb_t* fb = camera.capture();

    if (use_flash) {
        digitalWrite(FLASH_GPIO_NUM, LOW); // 拍摄后立即关闭闪光灯
        Serial.println("[Photo] Flash disabled after capture.");
    }

    if (!fb) {
        return false;
    }
    bool success = network.publishPhoto(fb->buf, fb->len);
    camera.release(fb);
    return success;
}

// ============================================================
//  Arduino 运行入口
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n====================================");
    Serial.println("             Water_Camera");
    Serial.println("====================================");

    // 初始化指示灯与闪光灯引脚
    pinMode(STATUS_LED_GPIO_NUM, OUTPUT);
    
    // 开机指示灯：亮起 1 秒后熄灭，指示系统成功上电并复位
    digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_ON);
    delay(1000);
    digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);

    pinMode(FLASH_GPIO_NUM, OUTPUT);
    digitalWrite(FLASH_GPIO_NUM, LOW);

    // 摄像头硬件复位：利用 PWDN 引脚进行硬件断电重启，确保冷启动时供电完全稳定
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, HIGH); // 关断摄像头电源
    delay(500);                        // 等待彻底放电
    digitalWrite(PWDN_GPIO_NUM, LOW);  // 开启摄像头电源
    delay(500);                        // 等待供电与时钟稳定

    // 先初始化摄像头硬件，确保供电稳定（WiFi 未启动时系统电流最低，电压最稳定）
    if (!camera.init()) {
        Serial.println("System halt due to camera init failure! Restarting in 3 seconds...");
        
        // 摄像头初始化失败：闪烁 3 次指示灯（200ms 亮 / 200ms 灭）作为硬件诊断指示
        for (int i = 0; i < 3; i++) {
            digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_ON);
            delay(200);
            digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);
            delay(200);
        }
        
        ESP.restart();
    }

    // 摄像头初始化成功后再初始化网络连接
    network.init();
    network.setMqttCallback(mqtt_callback);

    // 初始化 SNTP 网络时间对时 (北京时间 UTC+8)
    configTime(8 * 3600, 0, "ntp.aliyun.com", "time.nist.gov", "pool.ntp.org");
    Serial.println("[Time] SNTP configured.");

    s_last_photo_time_ms = millis(); // 初始化拍照时间戳

    delay(200);
}

void evaluate_brightness() {
    int yavg = camera.get_yavg();
    if (yavg >= 0) {
        if (yavg < AMBIENT_BRIGHTNESS_THRESHOLD) {
            s_use_flash_next_time = true;
            Serial.printf("[Evaluation] Brightness is low (%d < %d). Flash will be ENABLED next time.\n", 
                          yavg, AMBIENT_BRIGHTNESS_THRESHOLD);
        } else {
            s_use_flash_next_time = false;
            Serial.printf("[Evaluation] Brightness is sufficient (%d >= %d). Flash will be DISABLED next time.\n", 
                          yavg, AMBIENT_BRIGHTNESS_THRESHOLD);
        }
    } else {
        Serial.println("[Evaluation] Failed to read brightness. Keeping previous flash setting.");
    }
}

void loop() {
    unsigned long now = millis();
    static unsigned long s_last_loop_ms = 0;
    if (s_last_loop_ms == 0) {
        s_last_loop_ms = now;
    }

    // 运行网络维持状态机 (包含 WiFi 与 MQTT 非阻塞连接与轮询)
    network.loop(now);

    // 全局网络连接状态检查 (WiFi 与 MQTT 均连接成功)
    bool is_online = (WiFi.status() == WL_CONNECTED && network.isConnected());

    if (!is_online) {
        // 断网时暂停 60 秒空闲计时器（把上一次拍照时间戳顺延）
        s_last_photo_time_ms += (now - s_last_loop_ms);
    }
    s_last_loop_ms = now;

    // 监听全局网络连接状态的变化并打印日志
    static bool s_last_conn_status = false;
    if (is_online != s_last_conn_status) {
        s_last_conn_status = is_online;
        if (is_online) {
            Serial.println("[Network] Status Change: ONLINE (WiFi and MQTT are connected)");
        } else {
            Serial.println("[Network] Status Change: OFFLINE (WiFi or MQTT disconnected)");
            if (s_in_retry_mode) {
                Serial.println("[Photo] Network went offline. Pausing retry attempts...");
            }
        }
    }

    // 1. 处理首次拍照请求
    if (s_need_take_photo) {
        if (!is_online) {
            static unsigned long last_offline_print = 0;
            if (now - last_offline_print >= 5000) {
                last_offline_print = now;
                Serial.println("[Photo] Capture trigger pending: Waiting for Network ONLINE...");
            }
        } else {
            s_need_take_photo = false;
            Serial.println("[Photo] Starting initial photo job...");
            bool success = process_photo_job(s_use_flash_next_time);
            if (!success) {
                Serial.println("[Photo] Initial job failed. Entering retry mode...");
                s_in_retry_mode = true;
                s_retry_count = 0;
                s_last_capture_attempt_ms = now;
            } else {
                Serial.println("[Photo] Initial job succeeded!");
                s_last_photo_time_ms = now; // 成功拍摄更新时间戳
            }
        }
    }

    // 2. 处理重试逻辑
    if (s_in_retry_mode) {
        if (now - s_last_capture_attempt_ms >= RETRY_INTERVAL_MS) {
            if (!is_online) {
                Serial.println("[Photo] Retry timer fired, but network is OFFLINE. Postponing retry attempt for 10 seconds...");
                s_last_capture_attempt_ms = now; // 顺延 10 秒
            } else {
                s_retry_count++;
                Serial.printf("[Photo] Retrying photo job... Attempt %d of %d\n", s_retry_count, MAX_RETRY_ATTEMPTS);
                bool success = process_photo_job(s_use_flash_next_time);
                s_last_capture_attempt_ms = now;
                
                if (success) {
                    Serial.println("[Photo] Retry job succeeded!");
                    s_in_retry_mode = false;
                    s_last_photo_time_ms = now; // 成功拍摄更新时间戳
                } else {
                    if (s_retry_count >= MAX_RETRY_ATTEMPTS) {
                        Serial.println("[Photo] All retry attempts failed. Aborting.");
                        s_in_retry_mode = false;
                        s_last_photo_time_ms = now; // 全部失败亦更新时间戳，避免立刻触发空闲评估
                    }
                }
            }
        }
    }

    // 3. 开机 10 秒后的单次自检拍照上报
    static bool self_test_done = false;
    if (!self_test_done && now >= 10000) {
        self_test_done = true;
        Serial.println("[SelfTest] 10s boot self-test: triggering first photo...");
        trigger_photo_capture();
    }

    // 4. 检查是否连续 60 秒没有进行过任何拍摄（需处于在线状态，且无未决拍照任务或重试任务）
    if (is_online && !s_need_take_photo && !s_in_retry_mode) {
        if (now - s_last_photo_time_ms >= 60000UL) {
            Serial.println("[Idle] No photo captured for 60 seconds. Triggering local evaluation photo...");
            
            // 确保关闭闪光灯（评估拍摄强制不使用闪光灯）
            digitalWrite(FLASH_GPIO_NUM, LOW);
            
            // 本地拍照并释放，只用于评估亮度
            camera_fb_t* fb = camera.capture();
            if (fb) {
                evaluate_brightness();
                camera.release(fb);
            } else {
                Serial.println("[Idle] Failed to capture evaluation photo.");
            }
            
            // 无论拍照成功与否，更新拍照时间戳，重新开始计时
            s_last_photo_time_ms = now;
        }
    }
}
