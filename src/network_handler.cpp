#include "network_handler.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "config.h"

// 备用 DNS 解析：结合标准 DNS 与 HTTP-DNS，绕过本地代理劫持 (Fake-IP)
static IPAddress resolve_broker_ip() {
    IPAddress resolvedIP;
    
    // 1. 尝试使用标准的 DNS 解析
    if (WiFi.hostByName(MQTT_BROKER, resolvedIP)) {
        // 如果解析出来的 IP 属于 Clash 的 Fake-IP 范围 (198.18.x.x)，说明被本地代理劫持且 ESP32 无法直接路由
        if (resolvedIP[0] == 198 && resolvedIP[1] == 18) {
            Serial.printf("[DNS] Resolved to Fake-IP %s, bypassing...\n", resolvedIP.toString().c_str());
        } else {
            Serial.printf("[DNS] Successfully resolved %s to %s via standard DNS\n", MQTT_BROKER, resolvedIP.toString().c_str());
            return resolvedIP;
        }
    } else {
        Serial.printf("[DNS] Standard DNS failed for %s\n", MQTT_BROKER);
    }
    
    // 2. 备用方案：使用 HTTP-DNS (通过 TCP 80 端口直接查询 223.5.5.5，绕过本地 53 端口 DNS 劫持)
    Serial.println("[DNS] Attempting HTTP-DNS resolution via AliDNS...");
    HTTPClient http;
    String url = "http://223.5.5.5/resolve?name=" + String(MQTT_BROKER) + "&type=A";
    http.begin(url);
    http.setTimeout(3000); // 3 秒超时
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        int index = payload.indexOf("\"data\":\"");
        if (index != -1) {
            int start = index + 8;
            int end = payload.indexOf("\"", start);
            if (end != -1) {
                String ipStr = payload.substring(start, end);
                if (resolvedIP.fromString(ipStr.c_str())) {
                    Serial.printf("[DNS] HTTP-DNS successfully resolved %s to %s\n", MQTT_BROKER, resolvedIP.toString().c_str());
                    return resolvedIP;
                }
            }
        }
    } else {
        Serial.printf("[DNS] HTTP-DNS request failed, HTTP Code: %d\n", httpCode);
    }
    http.end();
    
    return IPAddress(0, 0, 0, 0);
}

// 共享状态变量，用于后台异步 DNS 解析与 MQTT 连接任务
static IPAddress s_resolved_broker_ip = IPAddress(0, 0, 0, 0);
static bool s_mqtt_connecting = false;
static unsigned long s_last_dns_resolve_ms = 0;

void mqtt_connect_task(void* pvParameters) {
    unsigned long now = millis();
    // 如果尚未解析成功过，或者解析缓存已超过 5 分钟，则触发 DNS/HTTP-DNS 解析
    if (s_resolved_broker_ip[0] == 0 || (now - s_last_dns_resolve_ms > 300000)) {
        s_last_dns_resolve_ms = now;
        IPAddress tempIP = resolve_broker_ip();
        if (tempIP[0] != 0) {
            s_resolved_broker_ip = tempIP;
            Serial.printf("[MQTT Task] DNS resolved IP: %s\n", tempIP.toString().c_str());
        } else {
            Serial.println("[MQTT Task] DNS resolution failed, will fallback to domain.");
        }
    }
    
    // 配置服务器地址
    if (s_resolved_broker_ip[0] != 0) {
        network._mqttClient.setServer(s_resolved_broker_ip, MQTT_PORT);
    } else {
        network._mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    }
    
    // 执行阻塞的 TCP 连接与 MQTT 握手
    Serial.println("[MQTT Task] Attempting connection to Broker...");
    String clientId = "ESP32Camera-";
    clientId += String(random(0xffff), HEX);
    
    if (network._mqttClient.connect(clientId.c_str())) {
        Serial.println("[MQTT Task] Connected successfully!");
        network._mqttClient.subscribe(MQTT_CMD_TOPIC);
        Serial.printf("[MQTT Task] Subscribed to topic: %s\n", MQTT_CMD_TOPIC);
    } else {
        Serial.printf("[MQTT Task] Connection failed, state = %d\n", network._mqttClient.state());
    }
    
    s_mqtt_connecting = false;
    vTaskDelete(NULL); // 连接任务完成，自我销毁
}

// 实例化全局单例
NetworkHandler network;

// 辅助状态翻译工具
static const char* wl_status_to_string(wl_status_t status) {
    switch (status) {
        case WL_NO_SHIELD: return "WL_NO_SHIELD (No WiFi hardware)";
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL (SSID not found)";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED (Wrong password?)";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED (Disconnected/connecting)";
        default: return "UNKNOWN";
    }
}

static const char* mqtt_state_to_string(int state) {
    switch (state) {
        case -4: return "MQTT_CONNECTION_TIMEOUT";
        case -3: return "MQTT_CONNECTION_LOST";
        case -2: return "MQTT_CONNECT_FAILED";
        case -1: return "MQTT_DISCONNECTED";
        case 0:  return "MQTT_CONNECTED";
        case 1:  return "MQTT_CONNECT_BAD_PROTOCOL";
        case 2:  return "MQTT_CONNECT_BAD_CLIENT_ID";
        case 3:  return "MQTT_CONNECT_UNAVAILABLE";
        case 4:  return "MQTT_CONNECT_BAD_CREDENTIALS";
        case 5:  return "MQTT_CONNECT_UNAUTHORIZED";
        default: return "UNKNOWN";
    }
}

static void scan_wifi_networks() {
    Serial.println("[WiFi] Scanning for available networks...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int n = WiFi.scanNetworks();
    Serial.println("[WiFi] Scan complete.");
    if (n == 0) {
        Serial.println("[WiFi] No networks found!");
    } else {
        Serial.printf("[WiFi] Found %d networks:\n", n);
        for (int i = 0; i < n; ++i) {
            Serial.printf("  %d: SSID: \"%s\", RSSI: %d dBm, Channel: %d, Encryption: %s\n", 
                          i + 1,
                          WiFi.SSID(i).c_str(), 
                          WiFi.RSSI(i), 
                          WiFi.channel(i),
                          WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "encrypted");
            delay(10);
        }
    }
    Serial.println("------------------------------------");
}

NetworkHandler::NetworkHandler() : _mqttClient(_espClient), _lastReconnectTime(0) {}

void NetworkHandler::init() {
    _wifiInit();
    
    // 动态解析 MQTT Broker 的 IP，以应对本地 DNS 劫持或 DDNS IP 发生变更的问题
    IPAddress brokerIP = resolve_broker_ip();
    if (brokerIP[0] != 0) {
        s_resolved_broker_ip = brokerIP; // 同步保存初始解析结果
        s_last_dns_resolve_ms = millis(); // 初始化解析时间戳
        _mqttClient.setServer(brokerIP, MQTT_PORT);
        Serial.printf("[MQTT] Server set to resolved IP: %s:%d\n", brokerIP.toString().c_str(), MQTT_PORT);
    } else {
        _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
        Serial.printf("[MQTT] DNS resolution failed, fallback to domain: %s:%d\n", MQTT_BROKER, MQTT_PORT);
    }

    // 申请 16KB 的 MQTT 缓冲区（由于长度 <= 32KB，将优先分配到内部高速 SRAM 中）
    if (!_mqttClient.setBufferSize(16384)) {
        Serial.println("[MQTT] WARN: 16KB SRAM alloc failed, falling back to 8KB");
        _mqttClient.setBufferSize(8192);
    } else {
        Serial.println("[MQTT] 16KB Buffer successfully allocated in internal SRAM.");
    }
}

void NetworkHandler::loop(unsigned long now) {
    _handleStatusLed(now);
    _handleWifiReconnect(now);

    if (WiFi.status() == WL_CONNECTED) {
        if (!_mqttClient.connected()) {
            _reconnectMqtt(now);
        } else {
            _mqttClient.loop();
        }
    }
}

bool NetworkHandler::publishPhoto(const uint8_t* data, size_t len) {
    if (!_mqttClient.connected()) {
        Serial.println("[MQTT] Disconnected, skip publishing.");
        return false;
    }

    Serial.printf("[MQTT] Publishing photo payload (%.1f KB) via streaming...\n", len / 1024.0f);
    if (_mqttClient.beginPublish(MQTT_DATA_TOPIC, len, false)) {
        // 分块写入，每块 512 字节，避免 TCP 发送缓冲区溢出
        const size_t CHUNK = 512;
        size_t sent = 0;
        while (sent < len) {
            size_t to_send = (len - sent) > CHUNK ? CHUNK : (len - sent);
            size_t written = _mqttClient.write(data + sent, to_send);
            if (written == 0) {
                Serial.println("[MQTT] Stream write error! Connection may have dropped.");
                break;
            }
            sent += written;
        }
        _mqttClient.endPublish();
        if (sent == len) {
            Serial.printf("[MQTT] Photo published successfully! (%.1f KB sent)\n", len / 1024.0f);
            return true;
        } else {
            Serial.printf("[MQTT] Publish FAILED! Only sent %.1f/%.1f KB.\n", sent / 1024.0f, len / 1024.0f);
        }
    } else {
        Serial.println("[MQTT] beginPublish FAILED! Buffer or connection issue.");
    }
    return false;
}

void NetworkHandler::setMqttCallback(void (*callback)(char*, byte*, unsigned int)) {
    _mqttClient.setCallback(callback);
}

bool NetworkHandler::isConnected() {
    return _mqttClient.connected();
}

void NetworkHandler::processMqtt() {
    if (_mqttClient.connected()) {
        _mqttClient.loop();
    }
}

void NetworkHandler::_wifiInit() {
    scan_wifi_networks();

    Serial.printf("[WiFi] Target SSID to connect: \"%s\" (Length: %zu)\n", WIFI_SSID, strlen(WIFI_SSID));
    Serial.printf("[WiFi] Target Password: \"%s\" (Length: %zu)\n", WIFI_PASSWORD, strlen(WIFI_PASSWORD));

    Serial.print("[WiFi] Connecting to target network...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.printf("[WiFi] Attempt %d: Status = %s\n", attempts + 1, wl_status_to_string(WiFi.status()));
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] Connected! IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.printf("[WiFi] Gateway: %s, Subnet: %s, DNS: %s\n", 
                      WiFi.gatewayIP().toString().c_str(), 
                      WiFi.subnetMask().toString().c_str(), 
                      WiFi.dnsIP().toString().c_str());
        Serial.print("[WiFi] RSSI (Signal Strength): ");
        Serial.println(WiFi.RSSI());
    } else {
        Serial.printf("[WiFi] Connect failed! Final Status = %s. Will retry in background.\n", wl_status_to_string(WiFi.status()));
    }
}

void NetworkHandler::_reconnectMqtt(unsigned long now) {
    if (now - _lastReconnectTime < MQTT_RECONNECT_INTERVAL_MS) {
        return; // 5秒连接尝试间隔节流
    }
    
    if (s_mqtt_connecting) {
        return; // 如果当前后台已经在尝试连接，则直接跳过，避免重复创建任务
    }
    
    _lastReconnectTime = now;
    
    // 启动后台异步解析与连接任务，并捕获结果
    BaseType_t ret = xTaskCreate(mqtt_connect_task, "mqtt_conn", 8192, NULL, 1, NULL);
    if (ret == pdPASS) {
        s_mqtt_connecting = true; // 任务创建成功，立即加锁
    } else {
        Serial.println("[MQTT] Error: Failed to create MQTT connection task!");
    }
}

void NetworkHandler::_handleStatusLed(unsigned long now) {
    // 根据当前连接状态动态设定闪烁周期（占空比均为 50%）
    unsigned long period = 0;
    if (WiFi.status() == WL_CONNECTED) {
        if (_mqttClient.connected()) {
            period = 1000; // WiFi 与 MQTT 均连接成功：1 秒周期 (快闪)
        } else {
            period = 2000; // 仅 WiFi 连接成功：2 秒周期 (中速闪烁)
        }
    } else {
        period = 0;        // WiFi 未连接：常灭
    }
    
    if (period == 0) {
        digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);
    } else {
        if ((now % period) < (period / 2)) {
            digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_ON);
        } else {
            digitalWrite(STATUS_LED_GPIO_NUM, STATUS_LED_OFF);
        }
    }
}

void NetworkHandler::_handleWifiReconnect(unsigned long now) {
    static unsigned long last_status_print = 0;
    static wl_status_t last_status = WL_NO_SHIELD;
    static unsigned long last_reconnect_attempt = 0;
    
    wl_status_t current_status = WiFi.status();
    
    if (current_status != last_status) {
        Serial.printf("[WiFi] Status changed: %s -> %s\n", 
                      wl_status_to_string(last_status), 
                      wl_status_to_string(current_status));
        last_status = current_status;
    }
    
    if (current_status != WL_CONNECTED) {
        if (now - last_status_print >= 10000) {
            last_status_print = now;
            Serial.printf("[WiFi Status Log] Currently disconnected. Status = %s\n", wl_status_to_string(current_status));
        }
        
        if (now - last_reconnect_attempt >= 20000) {
            last_reconnect_attempt = now;
            Serial.println("[WiFi] Connection timeout, initiating begin() again...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    } else {
        last_reconnect_attempt = now;
    }
}
