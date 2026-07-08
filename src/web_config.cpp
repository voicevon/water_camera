#include "web_config.h"
#include "nvs_config.h"
#include "index_html.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>

// ============================================================
//  Web 服务器实例（服务层私有）
// ============================================================
static WebServer s_server(80);

// ============================================================
//  REST API 处理函数
// ============================================================

// GET /api/sysconfig — 返回系统配置 JSON
static void handle_get_sysconfig() {
    String json = "{";
    json += "\"ssid\":\"" + get_sta_ssid() + "\",";
    json += "\"pass\":\"" + get_sta_password() + "\",";
    json += "\"name\":\"" + get_station_name() + "\",";
    json += "\"broker\":\"" + get_mqtt_broker() + "\",";
    json += "\"port\":" + String(get_mqtt_port());
    json += "}";
    s_server.send(200, "application/json", json);
}

// POST /api/sysconfig — 保存系统配置到 NVS
static void handle_post_sysconfig() {
    bool changed = false;
    if (s_server.hasArg("ssid"))     changed |= nvs_set_sta_ssid(s_server.arg("ssid"));
    if (s_server.hasArg("password")) changed |= nvs_set_sta_password(s_server.arg("password"));
    if (s_server.hasArg("name"))     changed |= nvs_set_station_name(s_server.arg("name"));
    if (s_server.hasArg("broker"))   changed |= nvs_set_mqtt_broker(s_server.arg("broker"));
    if (s_server.hasArg("port"))     changed |= nvs_set_mqtt_port(s_server.arg("port").toInt());

    if (changed) {
        Serial.println("[WebConfig] System configurations updated in NVS.");
    }
    s_server.send(200, "text/plain", "OK");
}

// GET /api/warmup — 返回闪光灯预热时间
static void handle_get_warmup() {
    String json = "{\"warmup\":" + String(get_warmup_sec(), 2) + "}";
    s_server.send(200, "application/json", json);
}

// POST /api/warmup — 保存闪光灯预热时间到 NVS
static void handle_post_warmup() {
    if (s_server.hasArg("warmup")) {
        float warmup = s_server.arg("warmup").toFloat();
        if (nvs_set_warmup_sec(warmup)) {
            Serial.printf("[WebConfig] Warm-up time updated to %.2f seconds\n", warmup);
            s_server.send(200, "text/plain", "OK");
            return;
        }
    }
    s_server.send(400, "text/plain", "Bad Request");
}

// GET /api/scan — 扫描附近 WiFi 并按信号强度排序返回
static void handle_wifi_scan() {
    int n = WiFi.scanNetworks(false, false);
    String json = "{\"networks\":[";
    if (n > 0) {
        int indices[n];
        for (int i = 0; i < n; i++) indices[i] = i;
        for (int i = 0; i < n - 1; i++) {
            for (int j = i + 1; j < n; j++) {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                    int temp = indices[i];
                    indices[i] = indices[j];
                    indices[j] = temp;
                }
            }
        }
        for (int i = 0; i < n; i++) {
            int idx = indices[i];
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(idx) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(idx));
            json += "}";
            if (i < n - 1) json += ",";
        }
    }
    json += "]}";
    WiFi.scanDelete();
    s_server.send(200, "application/json", json);
}

// ============================================================
//  公共接口实现
// ============================================================

void web_config_init() {
    // 1. 初始化 NVS 配置层（加载所有持久化参数）
    nvs_config_init();

    // 2. 启动 AP_STA 双模，开启软 AP 供配置接入
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("WaterCamera_AP", "12344321");
    Serial.printf("[WebConfig] SoftAP started. SSID: \"WaterCamera_AP\", IP: %s\n",
                  WiFi.softAPIP().toString().c_str());

    // 3. 注册路由
    s_server.on("/", HTTP_GET, []() {
        s_server.send_P(200, "text/html", INDEX_HTML);
    });
    s_server.on("/api/sysconfig", HTTP_GET,  handle_get_sysconfig);
    s_server.on("/api/sysconfig", HTTP_POST, handle_post_sysconfig);
    s_server.on("/api/wifi",      HTTP_GET,  handle_get_sysconfig);   // 兼容旧接口
    s_server.on("/api/wifi",      HTTP_POST, handle_post_sysconfig);  // 兼容旧接口
    s_server.on("/api/warmup",    HTTP_GET,  handle_get_warmup);
    s_server.on("/api/warmup",    HTTP_POST, handle_post_warmup);
    s_server.on("/api/scan",      HTTP_GET,  handle_wifi_scan);

    s_server.begin();
    Serial.println("[WebConfig] Embedded Web Server started on port 80");
}

void web_config_loop() {
    // 防回退机制：检测 WiFi 模式是否被外部库强制切回 STA
    if (WiFi.getMode() == WIFI_STA) {
        Serial.println("[WebConfig] WiFi mode was reverted to STA. Restoring AP_STA and restarting softAP...");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("WaterCamera_AP", "12344321");
    }
    s_server.handleClient();
}
