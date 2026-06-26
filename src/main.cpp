#include <Arduino.h>
#include "camera_handler.h"
#include "network_handler.h"
#include "config.h"

// 异步拍照任务标志位
static volatile bool s_need_take_photo = false;

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
        if (strcmp(payload_str, "take") == 0) {
            s_need_take_photo = true; // 异步触发
        }
    }
}

// ============================================================
//  拍摄并发送照片的业务逻辑
// ============================================================
void process_photo_job() {
    camera_fb_t* fb = camera.capture();
    if (!fb) {
        return;
    }
    network.publishPhoto(fb->buf, fb->len);
    camera.release(fb);
}

// ============================================================
//  Arduino 运行入口
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n====================================");
    Serial.println("ESP32-CAM Water Node Start (Refactored)");
    Serial.println("====================================");

    // 初始化指示灯与闪光灯引脚
    pinMode(STATUS_LED_GPIO_NUM, OUTPUT);
    digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);

    pinMode(FLASH_GPIO_NUM, OUTPUT);
    digitalWrite(FLASH_GPIO_NUM, LOW);

    // 先初始化网络连接
    network.init();
    network.setMqttCallback(mqtt_callback);

    // 初始化 SNTP 网络时间对时 (北京时间 UTC+8)
    configTime(8 * 3600, 0, "ntp.aliyun.com", "time.nist.gov", "pool.ntp.org");
    Serial.println("[Time] SNTP configured.");

    // 网络就绪后再初始化摄像头硬件，减少初始化时的 DMA 溢出窗口
    if (!camera.init()) {
        Serial.println("System halt due to camera init failure! Restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    
    delay(200);
}

void loop() {
    unsigned long now = millis();

    // 运行网络维持状态机 (包含 WiFi 与 MQTT 非阻塞连接与轮询)
    network.loop(now);

    if (WiFi.status() == WL_CONNECTED && network.isConnected()) {
        // 异步处理 MQTT 触发的拍照任务
        if (s_need_take_photo) {
            s_need_take_photo = false;
            process_photo_job();
        }

        // 开机 10 秒后的单次自检拍照上报
        static bool self_test_done = false;
        if (!self_test_done && now >= 10000) {
            self_test_done = true;
            Serial.println("[SelfTest] 10s boot self-test: capturing first photo...");
            process_photo_job();
        }
    }
}
