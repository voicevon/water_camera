#ifndef NETWORK_HANDLER_H
#define NETWORK_HANDLER_H

#include <WiFi.h>
#include <PubSubClient.h>

class NetworkHandler {
public:
    NetworkHandler();

    /**
     * @brief 初始化 WiFi 与 MQTT 设置
     */
    void init();

    /**
     * @brief 维持 WiFi 和 MQTT 连接的非阻塞循环
     * @param now 当前系统运行毫秒数
     */
    void loop(unsigned long now);

    /**
     * @brief 使用非拷贝分片式流式 API 发送大型 JPEG 数据包
     * @param data 数据指针
     * @param len 数据长度
     * @return bool 是否成功发布
     */
    bool publishPhoto(const uint8_t* data, size_t len);

    /**
     * @brief 发送小型文本消息到指定 MQTT Topic（用于报警、状态上报等）
     * @param topic MQTT 主题
     * @param payload 消息内容（空结尾字符串）
     * @return bool 是否成功发布
     */
    bool publishText(const char* topic, const char* payload);

    /**
     * @brief 设置 MQTT 回调函数
     * @param callback 传入的回调指针
     */
    void setMqttCallback(void (*callback)(char*, byte*, unsigned int));

    /**
     * @brief 获取 MQTT 客户端是否连接成功
     * @return bool 
     */
    bool isConnected();

    /**
     * @brief 驱动 MQTT loop
     */
    void processMqtt();

private:
    WiFiClient _espClient;
    PubSubClient _mqttClient;
    unsigned long _lastReconnectTime;
    SemaphoreHandle_t _mqttMutex;

    void _wifiInit();
    void _reconnectMqtt(unsigned long now);
    void _handleStatusLed(unsigned long now);
    void _handleWifiReconnect(unsigned long now);

    friend void mqtt_connect_task(void* pvParameters);
};

// 全局单例声明
extern NetworkHandler network;

#endif // NETWORK_HANDLER_H
