# water_camera

基于 **AI-Thinker ESP32-CAM** 模组的污水采样系统拍照节点固件。

设备连接到 MQTT Broker，监听来自中控系统或移动端的拍照指令，完成拍摄后将 JPEG 图像数据流式发布到指定主题，供下游系统接收存储或展示。

---

## 硬件

| 组件 | 型号 |
| :--- | :--- |
| 主控模组 | AI-Thinker ESP32-CAM |
| 摄像头 | 板载 OV2640 |
| 闪光灯 | GPIO 4（板载白色 LED） |
| 状态指示灯 | GPIO 33（板载红色 LED，低电平有效） |

---

## MQTT 主题说明

### 订阅（Subscribe）— 接收拍照指令

| 参数 | 值 |
| :--- | :--- |
| **Topic** | `water/photo/take` |
| **QoS** | 0（默认，Best Effort） |
| **Payload 类型** | 纯文本字符串 |
| **Payload 内容** | 目标站点名称，例如 `dongzhan` |

**触发逻辑**：当收到的 Payload 与 `config.h` 中的 `STATION_NAME`（默认 `"dongzhan"`）完全匹配时，设备才会触发拍照。不匹配的 Payload 将被静默忽略，支持多台相机混布于同一 Broker 而互不干扰。

**示例（MQTTX / mosquitto）**：
```
mosquitto_pub -h voicevon.vicp.io -t "water/photo/take" -m "dongzhan"
```

---

### 发布（Publish）— 上报照片数据

| 参数 | 值 |
| :--- | :--- |
| **Topic** | `water/photo/status/dongzhan` |
| **QoS** | 0（默认，Best Effort） |
| **Payload 类型** | 二进制流（JPEG 图像数据） |
| **Payload 内容** | 完整 JPEG 文件字节流，大小通常为 30–100 KB（VGA 分辨率） |
| **发布方式** | 分块流式（`beginPublish` / `write` / `endPublish`），避免一次性申请大内存 |

> **注意**：主题中的站点名称（`dongzhan`）来自 `config.h` 中的 `STATION_NAME`，不同站点的相机会向不同主题发布，下游订阅端可以通过通配符 `water/photo/status/+` 订阅所有相机的照片。

---

## 工作流程

```
1. 上电 → WiFi 连接 → MQTT Broker 连接 → 订阅 water/photo/take
2. 收到 MQTT 消息 → 比对 Payload 与 STATION_NAME
3. 匹配成功 → 评估环境亮度 → 按需开启闪光灯 → 执行拍摄
4. 拍摄成功 → 将 JPEG 数据流式发布到 water/photo/status/dongzhan
5. 失败时自动重试，最多 3 次，重试间隔 10 秒
```

---

## 关键配置（`src/config.h`）

| 宏定义 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `STATION_NAME` | `"dongzhan"` | 站点名，决定订阅过滤和发布主题后缀 |
| `CAMERA_FRAME_SIZE` | `FRAMESIZE_VGA` | 图像分辨率（640×480） |
| `CAMERA_JPEG_QUALITY` | `8` | JPEG 压缩质量（0-63，值越小质量越高） |
| `AMBIENT_BRIGHTNESS_THRESHOLD` | `80` | 亮度阈值，低于此值开启闪光灯 |
| `MQTT_BROKER` | `"voicevon.vicp.io"` | MQTT Broker 地址 |
| `MQTT_PORT` | `1883` | MQTT Broker 端口 |
