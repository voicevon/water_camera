#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "config.h"

// 网络通信对象
static WiFiClient    s_espClient;
static PubSubClient  s_mqttClient(s_espClient);
static unsigned long s_last_reconnect_time = 0;

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
    config.pixel_format = PIXFORMAT_JPEG;
    
    // 如果支持外置 PSRAM 则使用 VGA，否则降级为较小的 QVGA 减少内存开销
    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 10; // 0-63，数字越小质量越高
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // 初始化摄像头
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] Init failed, err: 0x%x\n", err);
        return false;
    }
    
    Serial.println("[Camera] Init successful!");
    return true;
}

// ============================================================
//  拍摄照片并通过 MQTT 发送
// ============================================================
void take_and_send_photo() {
    Serial.println("[Camera] Taking photo...");
    
    // 1. 先抓取一帧并释放，清空传感器旧缓存以保证获取当前实时画面
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
        esp_camera_fb_return(fb);
    }
    
    // 2. 捕获真正的新鲜帧画面
    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[Camera] Capture failed!");
        return;
    }
    
    Serial.printf("[Camera] Captured! Size: %u bytes\n", fb->len);
    
    // 3. 发布 JPEG 二进制报文
    if (s_mqttClient.connected()) {
        Serial.println("[MQTT] Publishing photo payload...");
        bool success = s_mqttClient.publish(MQTT_DATA_TOPIC, fb->buf, fb->len);
        if (success) {
            Serial.println("[MQTT] Photo published successfully!");
        } else {
            Serial.println("[MQTT] Publish FAILED! Packet size might exceed Limit.");
        }
    } else {
        Serial.println("[MQTT] Disconnected, skip publishing.");
    }
    
    // 4. 归还帧缓冲以防内存泄露
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
//  WiFi 初始化连接
// ============================================================
void wifi_init() {
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("[WiFi] Connected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println();
        Serial.println("[WiFi] Connect failed. Will retry in background.");
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
        Serial.print(" failed, rc=");
        Serial.println(s_mqttClient.state());
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

    // 初始化相机硬件
    if (!init_camera()) {
        Serial.println("System halt due to camera init failure!");
        while (1) { delay(1000); }
    }

    // 初始化网络与MQTT连接
    wifi_init();
    s_mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    s_mqttClient.setCallback(mqtt_callback);
}

void loop() {
    unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        if (!s_mqttClient.connected()) {
            reconnect_mqtt(now);
        } else {
            s_mqttClient.loop();
        }
    }
}
