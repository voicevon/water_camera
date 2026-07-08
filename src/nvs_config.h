#pragma once

#include <Arduino.h>

/**
 * @brief 初始化 NVS 存储并加载所有配置项到内存缓存
 *        由 web_config_init() 调用，外部无需直接调用
 */
void nvs_config_init();

/**
 * @brief 配置写入接口 — 供 REST POST handler 调用
 *        各函数在值未变化时直接返回 false，跳过 NVS 写入
 *        返回值：true = 值已更新并写入 NVS；false = 无变化或非法值
 */
bool nvs_set_sta_ssid(const String& val);
bool nvs_set_sta_password(const String& val);
bool nvs_set_warmup_sec(float val);
bool nvs_set_station_name(const String& val);
bool nvs_set_mqtt_broker(const String& val);
bool nvs_set_mqtt_port(int val);
