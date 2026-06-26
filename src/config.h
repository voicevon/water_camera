#ifndef CONFIG_H
#define CONFIG_H

// -------- WiFi 网络配置 --------
#define WIFI_SSID       "Perfect"
#define WIFI_PASSWORD   "12344321"

// -------- MQTT Broker 配置 --------
#define MQTT_BROKER     "voicevon.vicp.io"
#define MQTT_PORT       1883

// -------- 站点名称配置 --------
#define STATION_NAME    "dongzhan"

// -------- MQTT Topic 配置 --------
#define MQTT_CMD_TOPIC  "water/photo/take"
#define MQTT_DATA_TOPIC "water/photo/status/" STATION_NAME

// MQTT 非阻塞重连最小间隔（毫秒）
#define MQTT_RECONNECT_INTERVAL_MS  5000UL

// -------- 闪光灯引脚配置 --------
#define FLASH_GPIO_NUM    4

// -------- LED 指示灯引脚配置 --------
#define STATUS_LED_GPIO_NUM  33   // 板载红色指示灯 (低电平有效)
#define STATUS_LED_ON        LOW
#define STATUS_LED_OFF       HIGH

// -------- AI-Thinker ESP32-CAM 摄像头引脚定义 --------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#endif // CONFIG_H
