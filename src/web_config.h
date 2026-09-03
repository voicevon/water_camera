#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>

/**
 * @brief 初始化 Web Server 并开启 AP+STA 共存模式，加载 NVS 参数
 */
void web_config_init();

/**
 * @brief 在主循环中驱动 Web Server 客户端请求和防模式回退守卫
 */
void web_config_loop();

#endif // WEB_CONFIG_H

