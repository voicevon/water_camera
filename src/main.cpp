#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "esp_camera.h"
#include "img_converters.h"
#include "font_watermark.h"
#include "config.h"

// 网络通信对象
static WiFiClient    s_espClient;
static PubSubClient  s_mqttClient(s_espClient);
static unsigned long s_last_reconnect_time = 0;

// ============================================================
//  像素点阵渲染算法
// ============================================================

// 绘制 16x16 汉字点阵
void draw_chinese_char_rgb565(uint16_t *buf, int width, int height, int x, int y, const uint8_t *bitmap, uint16_t color) {
    for (int r = 0; r < 16; r++) {
        uint16_t row_bits = (bitmap[r * 2] << 8) | bitmap[r * 2 + 1];
        for (int col = 0; col < 16; col++) {
            if ((row_bits >> (15 - col)) & 1) {
                int px = x + col;
                int py = y + r;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    buf[py * width + px] = color;
                }
            }
        }
    }
}

// 绘制 8x8 ASCII 字符点阵
void draw_char_rgb565(uint16_t *buf, int width, int height, int x, int y, char c, uint16_t color) {
    if (c < 0 || c >= 128) return;
    for (int r = 0; r < 8; r++) {
        uint8_t row_bits = font8x8_basic[(unsigned char)c][r];
        for (int col = 0; col < 8; col++) {
            if ((row_bits >> col) & 1) {
                int px = x + col;
                int py = y + r;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    buf[py * width + px] = color;
                }
            }
        }
    }
}

// 绘制字符串
void draw_string_rgb565(uint16_t *buf, int width, int height, int x, int y, const char *str, uint16_t color) {
    while (*str) {
        draw_char_rgb565(buf, width, height, x, y, *str, color);
        x += 8;
        str++;
    }
}

// 在 RGB565 帧缓冲区上绘制带双层阴影的水印
void draw_watermark(uint16_t *buf, int width, int height, const char* time_str) {
    // 水印起算坐标（右下角，宽约 224，高 16）
    int start_x = width - 240;
    int start_y = height - 30;

    // 1. 绘制汉字 "水路监控" (4个汉字，间隔16像素，共64像素)
    // 阴影层 (黑色)
    draw_chinese_char_rgb565(buf, width, height, start_x + 0*16 + 1, start_y + 1, chs_shui, 0x0000);
    draw_chinese_char_rgb565(buf, width, height, start_x + 1*16 + 1, start_y + 1, chs_lu,   0x0000);
    draw_chinese_char_rgb565(buf, width, height, start_x + 2*16 + 1, start_y + 1, chs_jian, 0x0000);
    draw_chinese_char_rgb565(buf, width, height, start_x + 3*16 + 1, start_y + 1, chs_kong, 0x0000);
    // 前景层 (白色)
    draw_chinese_char_rgb565(buf, width, height, start_x + 0*16,     start_y,     chs_shui, 0xFFFF);
    draw_chinese_char_rgb565(buf, width, height, start_x + 1*16,     start_y,     chs_lu,   0xFFFF);
    draw_chinese_char_rgb565(buf, width, height, start_x + 2*16,     start_y,     chs_jian, 0xFFFF);
    draw_chinese_char_rgb565(buf, width, height, start_x + 3*16,     start_y,     chs_kong, 0xFFFF);

    // 2. 绘制时间戳 (偏移 68 像素，垂直向下微调 4 像素以对齐汉字)
    int time_x = start_x + 68;
    int time_y = start_y + 4;
    // 阴影层
    draw_string_rgb565(buf, width, height, time_x + 1, time_y + 1, time_str, 0x0000);
    // 前景层
    draw_string_rgb565(buf, width, height, time_x,     time_y,     time_str, 0xFFFF);
}

// ============================================================
//  摄像头硬件初始化
// ============================================================
bool init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    
    // 如果支持外置 PSRAM 则使用 SVGA，并将格式设为 RGB565 裸帧以便添加水印
    if (psramFound()) {
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_XGA;    // XGA(1024x768) - PSRAM 缓冲区支持此分辨率
        config.jpeg_quality = 6;              // 质量提升: 10 → 6 (0-63，越小质量越高)
        config.fb_count = 1;
    } else {
        config.pixel_format = PIXFORMAT_JPEG; // 无 PSRAM 降级直接硬件输出 JPEG，无法添加水印
        config.frame_size = FRAMESIZE_VGA;    // 无PSRAM时退而求其次，使用 VGA
        config.jpeg_quality = 10;
        config.fb_count = 1;
    }

    // 初始化摄像头
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] Init failed, err: 0x%x\n", err);
        return false;
    }
    
    // 调整 OV2640 传感器图像质量参数
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 0);     // 亮度: -2 到 +2，0 为默认
        s->set_contrast(s, 1);       // 对比度: -2 到 +2，+1 提升对比度
        s->set_saturation(s, 0);     // 饱和度: -2 到 +2，0 为自然色彩
        s->set_whitebal(s, 1);       // 自动白平衡: 开启
        s->set_awb_gain(s, 1);       // AWB 增益: 开启
        s->set_wb_mode(s, 0);        // 白平衡模式: 0=自动, 1=晴天, 2=阴天, 3=办公室, 4=室内
        s->set_exposure_ctrl(s, 1);  // 自动曝光: 开启
        s->set_aec2(s, 1);           // AEC DSP 增强: 开启
        s->set_gain_ctrl(s, 1);      // 自动增益控制: 开启
        s->set_agc_gain(s, 0);       // AGC 增益级别: 0=最低噪導
        s->set_gainceiling(s, (gainceiling_t)2); // 增益上限: 2x (0–5)
        s->set_bpc(s, 1);            // 坏点修正: 开启
        s->set_wpc(s, 1);            // 白点修正: 开启
        s->set_raw_gma(s, 1);        // Gamma 修正: 开启
        s->set_lenc(s, 1);           // 镜头付款补偿: 开启（消除暗角现象）
        s->set_hmirror(s, 0);        // 水平镜像: 关闭
        s->set_vflip(s, 0);          // 垂直翻转: 关闭
        Serial.println("[Camera] Sensor tuning applied.");
    }
    
    Serial.println("[Camera] Init successful!");
    return true;
}

// ============================================================
//  拍摄照片、添加本地水印并通过 MQTT 发送
// ============================================================
void take_and_send_photo() {
    Serial.println("[Camera] Turning on flash...");
    digitalWrite(FLASH_GPIO_NUM, HIGH);
    delay(800); // 延长至 800ms，确保 OV2640 AE 曝光算法在闪光灯亮起后充分收敛
    
    Serial.println("[Camera] Taking photo...");
    
    // 1. 先抓取一帧并释放，清空传感器旧缓存以保证获取当前真实画面
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
        esp_camera_fb_return(fb);
    }
    
    // 2. 捕获真正的新鲜帧画面
    fb = esp_camera_fb_get();
    
    // 拍照完成，立即关闭闪光灯防热
    digitalWrite(FLASH_GPIO_NUM, LOW);
    
    if (!fb) {
        Serial.println("[Camera] Capture failed!");
        return;
    }
    
    Serial.printf("[Camera] Captured! Size: %u bytes, Format: %d\n", fb->len, fb->format);
    
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    bool compress_success = false;

    // 3. 只有当帧为 RGB565 格式时才绘制本地水印并调用软件压缩
    if (fb->format == PIXFORMAT_RGB565) {
        // 获取当前本地时间
        struct tm timeinfo;
        char time_str[32] = "1970-01-01 00:00:00";
        if (getLocalTime(&timeinfo)) {
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        }
        
        // 叠加本地水印 (汉字+时间)
        draw_watermark((uint16_t *)fb->buf, fb->width, fb->height, time_str);

        // 调用软件 JPEG 编码器压缩（质量参数与 init_camera 中保持一致）
        compress_success = fmt2jpg(fb->buf, fb->width * fb->height * 2, fb->width, fb->height, PIXFORMAT_RGB565, 6, &jpg_buf, &jpg_len);
        Serial.printf("[Camera] JPEG encoded: %ux%u, size=%u bytes\n", fb->width, fb->height, jpg_len);
    } else {
        // 硬件 JPEG 格式直接发布，无法加水印
        jpg_buf = fb->buf;
        jpg_len = fb->len;
        compress_success = true;
    }

    // 4. 发布 JPEG 二进制报文
    if (compress_success && jpg_buf && jpg_len > 0) {
        if (s_mqttClient.connected()) {
            Serial.printf("[MQTT] Publishing photo payload (%u bytes)...\n", jpg_len);
            bool success = s_mqttClient.publish(MQTT_DATA_TOPIC, jpg_buf, jpg_len);
            if (success) {
                Serial.println("[MQTT] Photo published successfully!");
            } else {
                Serial.println("[MQTT] Publish FAILED! Packet size might exceed Limit.");
            }
        } else {
            Serial.println("[MQTT] Disconnected, skip publishing.");
        }
    } else {
        Serial.println("[Camera] JPEG compression failed!");
    }
    
    // 5. 释放由 fmt2jpg 临时分配的 JPEG 缓冲区以及归还帧缓冲
    if (fb->format == PIXFORMAT_RGB565 && jpg_buf) {
        free(jpg_buf);
    }
    esp_camera_fb_return(fb);
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
        if (strcmp(payload_str, "take") == 0) {
            take_and_send_photo();
        }
    }
}

// ============================================================
//  调试辅助：WiFi 状态转字符串
// ============================================================
const char* wl_status_to_string(wl_status_t status) {
    switch (status) {
        case WL_NO_SHIELD: return "WL_NO_SHIELD (No WiFi hardware)";
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL (SSID not found)";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED (Wrong password?)";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED (Disconnected/connecting)";
        default: return "UNKNOWN";
    }
}

// ============================================================
//  调试辅助：MQTT 状态转字符串
// ============================================================
const char* mqtt_state_to_string(int state) {
    switch (state) {
        case -4: return "MQTT_CONNECTION_TIMEOUT";
        case -3: return "MQTT_CONNECTION_LOST";
        case -2: return "MQTT_CONNECT_FAILED";
        case -1: return "MQTT_DISCONNECTED";
        case 0:  return "MQTT_CONNECTED";
        case 1:  return "MQTT_CONNECT_BAD_PROTOCOL";
        case 2:  return "MQTT_CONNECT_BAD_CLIENT_ID";
        case 3:  return "MQTT_CONNECT_UNAVAILABLE";
        case 4:  return "MQTT_CONNECT_BAD_CREDENTIALS";
        case 5:  return "MQTT_CONNECT_UNAUTHORIZED";
        default: return "UNKNOWN";
    }
}

// ============================================================
//  WiFi 扫描并输出所有可见网络 (调试辅助)
// ============================================================
void scan_wifi_networks() {
    Serial.println("[WiFi] Scanning for available networks...");
    // 临时切换到 STATION 模式以确保扫描正常工作
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int n = WiFi.scanNetworks();
    Serial.println("[WiFi] Scan complete.");
    if (n == 0) {
        Serial.println("[WiFi] No networks found!");
    } else {
        Serial.printf("[WiFi] Found %d networks:\n", n);
        for (int i = 0; i < n; ++i) {
            Serial.printf("  %d: SSID: \"%s\", RSSI: %d dBm, Channel: %d, Encryption: %s\n", 
                          i + 1,
                          WiFi.SSID(i).c_str(), 
                          WiFi.RSSI(i), 
                          WiFi.channel(i),
                          WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "encrypted");
            delay(10);
        }
    }
    Serial.println("------------------------------------");
}

// ============================================================
//  WiFi 初始化连接
// ============================================================
void wifi_init() {
    // 1. 首先进行 WiFi 扫描并输出周围所有的 SSID 供开发者调试
    scan_wifi_networks();

    // 2. 打印当前的目标连接参数以校验是否存在空格/不可见字符
    Serial.printf("[WiFi] Target SSID to connect: \"%s\" (Length: %zu)\n", WIFI_SSID, strlen(WIFI_SSID));
    Serial.printf("[WiFi] Target Password: \"%s\" (Length: %zu)\n", WIFI_PASSWORD, strlen(WIFI_PASSWORD));

    Serial.print("[WiFi] Connecting to target network...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.printf("[WiFi] Attempt %d: Status = %s\n", attempts + 1, wl_status_to_string(WiFi.status()));
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] Connected! IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WiFi] RSSI (Signal Strength): ");
        Serial.println(WiFi.RSSI());
    } else {
        Serial.printf("[WiFi] Connect failed! Final Status = %s. Will retry in background.\n", wl_status_to_string(WiFi.status()));
    }
}

// ============================================================
//  非阻塞 MQTT 重连
// ============================================================
void reconnect_mqtt(unsigned long current_time) {
    if (current_time - s_last_reconnect_time < MQTT_RECONNECT_INTERVAL_MS) {
        return; // 5秒重连节流
    }
    s_last_reconnect_time = current_time;

    Serial.print("[MQTT] Connecting...");
    String clientId = "ESP32Camera-";
    clientId += String(random(0xffff), HEX);

    if (s_mqttClient.connect(clientId.c_str())) {
        Serial.println(" connected.");
        s_mqttClient.subscribe(MQTT_CMD_TOPIC);
        Serial.printf("[MQTT] Subscribed to topic: %s\n", MQTT_CMD_TOPIC);
    } else {
        Serial.printf(" failed, state = %s\n", mqtt_state_to_string(s_mqttClient.state()));
    }
}

// ============================================================
//  主运行骨架
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n====================================");
    Serial.println("ESP32-CAM Water Node Start");
    Serial.println("====================================");

    // 初始化指示灯与闪光灯引脚
    pinMode(STATUS_LED_GPIO_NUM, OUTPUT);
    digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);

    pinMode(FLASH_GPIO_NUM, OUTPUT);
    digitalWrite(FLASH_GPIO_NUM, LOW);

    // 初始化相机硬件
    if (!init_camera()) {
        Serial.println("System halt due to camera init failure!");
        while (1) { delay(1000); }
    }

    // 初始化网络与MQTT连接
    wifi_init();
    s_mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    s_mqttClient.setCallback(mqtt_callback);

    // 初始化 SNTP 网络时间对时 (北京时间 UTC+8)
    configTime(8 * 3600, 0, "ntp.aliyun.com", "time.nist.gov", "pool.ntp.org");
    Serial.println("[Time] SNTP configured. Time synchronization in progress...");
}

// 处理状态指示灯闪烁逻辑
void handle_status_led(unsigned long now) {
    static unsigned long last_blink_time = 0;
    static bool led_on = false;
    
    // 根据当前连接状态动态设定闪烁周期（占空比均为 50%）
    unsigned long period = 5000;
    if (WiFi.status() == WL_CONNECTED) {
        if (s_mqttClient.connected()) {
            period = 1000; // WiFi 与 MQTT 均连接成功：1 秒周期 (快闪表示正常)
        } else {
            period = 2000; // 仅 WiFi 连接成功：2 秒周期 (中速闪烁)
        }
    } else {
        period = 5000;     // WiFi 未连接（两者都未成功）：5 秒周期 (慢闪)
    }
    
    unsigned long half_period = period / 2;
    unsigned long elapsed = now - last_blink_time;
    
    // 如果状态变化导致已流过的时间超出当前的周期限制，执行状态重置防卡死
    if (elapsed >= period) {
        elapsed = 0;
        last_blink_time = now;
        led_on = false;
        digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);
    }
    
    if (led_on) {
        // 当前是亮着的状态，亮够半周期后关闭
        if (elapsed >= half_period) {
            led_on = false;
            last_blink_time = now;
            digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);
        }
    } else {
        // 当前是灭着的状态，灭够半周期后开启
        if (elapsed >= half_period) {
            led_on = true;
            last_blink_time = now;
            digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_ON);
        }
    }
}

// 处理 WiFi 重连及状态日志监测
void handle_wifi_reconnect(unsigned long now) {
    static unsigned long last_status_print = 0;
    static wl_status_t last_status = WL_NO_SHIELD;
    static unsigned long last_reconnect_attempt = 0;
    
    wl_status_t current_status = WiFi.status();
    
    // 状态发生改变时立即打印日志
    if (current_status != last_status) {
        Serial.printf("[WiFi] Status changed: %s -> %s\n", 
                      wl_status_to_string(last_status), 
                      wl_status_to_string(current_status));
        last_status = current_status;
    }
    
    if (current_status != WL_CONNECTED) {
        // 如果未连接，每10秒打印一次当前状态以供调试
        if (now - last_status_print >= 10000) {
            last_status_print = now;
            Serial.printf("[WiFi Status Log] Currently disconnected. Status = %s\n", wl_status_to_string(current_status));
        }
        
        // 如果断网持续超过 20 秒，强行调用 begin 重新发起连接
        if (now - last_reconnect_attempt >= 20000) {
            last_reconnect_attempt = now;
            Serial.println("[WiFi] Connection timeout, initiating begin() again...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    } else {
        // 连接正常时，重置重连计时器
        last_reconnect_attempt = now;
    }
}

void loop() {
    unsigned long now = millis();
    
    // 运行状态灯处理（错误时周期闪烁）
    handle_status_led(now);

    // 运行 WiFi 重连及日志输出
    handle_wifi_reconnect(now);

    if (WiFi.status() == WL_CONNECTED) {
        if (!s_mqttClient.connected()) {
            reconnect_mqtt(now);
        } else {
            s_mqttClient.loop();

            // 开机10秒后的单次自检拍照上报
            static bool self_test_done = false;
            if (!self_test_done && now >= 10000) {
                self_test_done = true;
                Serial.println("[SelfTest] 10s boot self-test: capturing first photo...");
                take_and_send_photo();
            }
        }
    }
}
