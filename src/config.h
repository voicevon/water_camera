#ifndef CONFIG_H
#define CONFIG_H

// -------- WiFi 网络配置 --------
#define WIFI_SSID       "Perfect"
#define WIFI_PASSWORD   "12344321"

// -------- MQTT Broker 配置 --------
#define MQTT_BROKER     "voicevon.vicp.io"
#define MQTT_PORT       1883

// -------- 站点及图像配置 (拍照目标与画质) --------
#define STATION_NAME    "dongzhan"  // 拍照的目标站点名称 (当 MQTT 指令内容与该站点匹配时触发拍照)

// 帧大小配置（图像尺寸分辨率）
// 可选分辨率：FRAMESIZE_UXGA (1600x1200), FRAMESIZE_SXGA (1280x1024), FRAMESIZE_XGA (1024x768), 
//            FRAMESIZE_SVGA (800x600), FRAMESIZE_VGA (640x480), FRAMESIZE_QVGA (320x240) 等
#define CAMERA_FRAME_SIZE       FRAMESIZE_VGA  // 默认使用较小的 VGA (640x480) 尺寸，可根据需要调整
// #define CAMERA_FRAME_SIZE       FRAMESIZE_SVGA  // 默认使用较小的 VGA (640x480) 尺寸，可根据需要调整

// JPEG 压缩画质配置
// 取值范围：0-63。数值越小图片质量越高/体积越大，数值越大压缩率越高/文件越小。
// 建议：大压缩率可设为 12 - 20
#define CAMERA_JPEG_QUALITY     8  // 默认设为 12，提供较大的压缩率以减小文件体积

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
