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

    void _wifiInit();
    void _reconnectMqtt(unsigned long now);
    void _handleStatusLed(unsigned long now);
    void _handleWifiReconnect(unsigned long now);

    friend void mqtt_connect_task(void* pvParameters);
};

// 全局单例声明
extern NetworkHandler network;

#endif // NETWORK_HANDLER_H
