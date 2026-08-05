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
bool nvs_set_brightness_thresh(int val);
bool nvs_set_station_name(const String& val);
bool nvs_set_mqtt_broker(const String& val);
bool nvs_set_mqtt_port(int val);

/**
 * @brief 最低环境亮度阈值 Getter（用于自动闪光灯触发评估，范围 0-255，默认 80）
 */
int   get_brightness_thresh();

/**
 * @brief 突变检测参数 Getter（从内存缓存读取，实时生效）
 */
bool  get_mutation_enable();
int   get_mutation_interval_sec();
float get_mutation_block_thresh();
int   get_mutation_min_blocks();

/**
 * @brief 突变检测参数 Setter（值变化时写入 NVS 并更新内存缓存）
 *        返回 true = 值已更新；false = 无变化或非法值
 */
bool nvs_set_mutation_enable(bool val);
bool nvs_set_mutation_interval_sec(int val);
bool nvs_set_mutation_block_thresh(float val);
bool nvs_set_mutation_min_blocks(int val);
