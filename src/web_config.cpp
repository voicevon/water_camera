#include "web_config.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Web 服务器实例
static WebServer s_server(80);
static Preferences s_prefs;

// 配置项变量
static String s_sta_ssid = "";
static String s_sta_password = "";
static float s_warmup_sec = 0.8f;

// 极简暗黑风格 SPA 前端 HTML
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>相机节点配置</title>
    <style>
        :root {
            --bg-main: #0B0F19;
            --bg-card: #151B2C;
            --border-color: #242F47;
            --accent-blue: #38BDF8;
            --text-main: #E2E8F0;
            --text-muted: #94A3B8;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background-color: var(--bg-main);
            color: var(--text-main);
            padding: 1.5rem;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }

        .container {
            width: 100%;
            max-width: 450px;
        }

        header {
            margin-bottom: 2rem;
            text-align: center;
        }

        header h1 {
            font-size: 1.5rem;
            font-weight: 700;
            color: var(--accent-blue);
            margin-bottom: 0.5rem;
        }

        header p {
            font-size: 0.85rem;
            color: var(--text-muted);
        }

        .card {
            background-color: var(--bg-card);
            border: 1px solid var(--border-color);
            border-radius: 16px;
            padding: 1.75rem;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4);
            margin-bottom: 1.5rem;
        }

        .card-title {
            font-size: 1.1rem;
            font-weight: 600;
            margin-bottom: 1.25rem;
            color: var(--accent-blue);
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 0.5rem;
        }

        .form-group {
            margin-bottom: 1.25rem;
        }

        .form-group label {
            display: block;
            font-size: 0.85rem;
            font-weight: 500;
            margin-bottom: 0.5rem;
            color: var(--text-muted);
        }

        .form-group input {
            width: 100%;
            padding: 0.75rem 1rem;
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            color: var(--text-main);
            font-size: 0.95rem;
            transition: border-color 0.2s;
        }

        .form-group input:focus {
            outline: none;
            border-color: var(--accent-blue);
        }

        .btn {
            width: 100%;
            padding: 0.75rem;
            background-color: var(--accent-blue);
            color: #0B0F19;
            border: none;
            border-radius: 8px;
            font-size: 0.95rem;
            font-weight: 600;
            cursor: pointer;
            transition: opacity 0.2s;
            box-shadow: 0 4px 12px rgba(56, 189, 248, 0.25);
        }

        .btn:hover {
            opacity: 0.9;
        }

        #toast {
            position: fixed;
            bottom: 2rem;
            left: 50%;
            transform: translateX(-50%) translateY(200px);
            opacity: 0;
            visibility: hidden;
            background: rgba(16, 185, 129, 0.95);
            color: white;
            padding: 0.75rem 1.75rem;
            border-radius: 50px;
            font-weight: 600;
            box-shadow: 0 8px 24px rgba(16, 185, 129, 0.3);
            transition: transform 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275), opacity 0.25s, visibility 0.25s;
            pointer-events: none;
            z-index: 1000;
            font-size: 0.9rem;
            text-align: center;
        }

        #toast.show {
            transform: translateX(-50%) translateY(0);
            opacity: 1;
            visibility: visible;
        }

        .wifi-list {
            margin-top: 0.5rem;
            max-height: 150px;
            overflow-y: auto;
            border: 1px solid var(--border-color);
            border-radius: 8px;
            background: rgba(0, 0, 0, 0.2);
            display: none;
        }
        .wifi-list.show {
            display: block;
        }
        .wifi-item {
            padding: 0.6rem 1rem;
            border-bottom: 1px solid var(--border-color);
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            font-size: 0.88rem;
            transition: background 0.2s;
        }
        .wifi-item:last-child {
            border-bottom: none;
        }
        .wifi-item:hover {
            background: rgba(255, 255, 255, 0.05);
        }
        .wifi-signal {
            color: var(--accent-blue);
        }

        .footer {
            text-align: center;
            font-size: 0.8rem;
            color: var(--text-muted);
            margin-top: 2rem;
            border-top: 1px solid var(--border-color);
            padding-top: 1rem;
        }
    </style>
</head>
<body>

    <div class="container">
        <header>
            <h1>智能相机节点</h1>
            <p>AP+STA 双模在线配置与管理</p>
        </header>

        <!-- WiFi 配置 -->
        <div class="card">
            <div class="card-title">外部 Wi-Fi 配置 (STA)</div>
            <form id="wifi-form" onsubmit="saveWifi(event)">
                <div class="form-group">
                    <label for="ssid">Wi-Fi 网络名称 (SSID)</label>
                    <div style="display: flex; gap: 0.5rem;">
                        <input type="text" id="ssid" name="ssid" placeholder="输入外部 Wi-Fi SSID" style="flex: 1;" required>
                        <button type="button" class="btn" style="width: auto; padding: 0.5rem 1rem; font-size: 0.85rem;" onclick="scanWifi(this)">扫描</button>
                    </div>
                    <div id="wifi-list" class="wifi-list"></div>
                </div>
                <div class="form-group">
                    <label for="password">Wi-Fi 网络密码 (Password)</label>
                    <input type="password" id="password" name="password" placeholder="输入外部 Wi-Fi 密码">
                </div>
                <button type="submit" class="btn">保存网络配置</button>
            </form>
        </div>

        <!-- 相机闪光灯预热配置 -->
        <div class="card">
            <div class="card-title">相机拍照参数</div>
            <form id="warmup-form" onsubmit="saveWarmup(event)">
                <div class="form-group">
                    <label for="warmup">闪光灯预热延迟 (Warm-up Time)</label>
                    <div style="display: flex; gap: 0.5rem; align-items: center;">
                        <input type="number" id="warmup" name="warmup" step="0.1" min="0.1" max="10.0" placeholder="0.8" style="flex: 1;" required>
                        <span style="color: var(--text-muted); font-size: 0.95rem;">秒</span>
                    </div>
                    <span style="font-size: 0.72rem; color: var(--text-muted); margin-top: 0.25rem; display: block;">亮灯至快门开启的时间间隔（如 0.8、1.5，秒为单位），用于自动白平衡与曝光收敛稳定</span>
                </div>
                <button type="submit" class="btn">保存拍照参数</button>
            </form>
        </div>

        <div class="footer">
            <p>设备型号: Water Camera Node (ESP32-CAM)</p>
            <p>版本信息: Version 1.0 (2026年7月)</p>
            <p>版权所有 © 山东卷烟厂 & 技术支持：山东卷积分公司</p>
        </div>
    </div>

    <div id="toast">保存成功，已即时生效！</div>

    <script>
        // 页面初始化时拉取配置数据
        async function fetchConfig() {
            try {
                const res = await fetch('/api/wifi');
                const data = await res.json();
                document.getElementById('ssid').value = data.ssid || '';
                document.getElementById('password').value = data.pass || '';
            } catch (err) {
                console.error("Fetch wifi failed:", err);
            }

            try {
                const res = await fetch('/api/warmup');
                const data = await res.json();
                document.getElementById('warmup').value = data.warmup !== undefined ? data.warmup : 0.8;
            } catch (err) {
                console.error("Fetch warmup failed:", err);
            }
        }

        async function saveWifi(e) {
            e.preventDefault();
            const form = document.getElementById('wifi-form');
            const params = new URLSearchParams(new FormData(form));
            try {
                const res = await fetch('/api/wifi', { method: 'POST', body: params });
                if (res.ok) {
                    showToast("Wi-Fi 配置已保存，设备将在后台连接！");
                }
            } catch (err) {
                alert("出错: " + err);
            }
        }

        async function saveWarmup(e) {
            e.preventDefault();
            const form = document.getElementById('warmup-form');
            const params = new URLSearchParams(new FormData(form));
            try {
                const res = await fetch('/api/warmup', { method: 'POST', body: params });
                if (res.ok) {
                    showToast("闪光灯预热时间已更新，立即生效！");
                }
            } catch (err) {
                alert("出错: " + err);
            }
        }

        function showToast(msg) {
            const toast = document.getElementById('toast');
            toast.textContent = msg;
            toast.className = 'show';
            setTimeout(() => { toast.className = ''; }, 2500);
        }

        async function scanWifi(btn) {
            const origText = btn.textContent;
            btn.textContent = "扫描中...";
            btn.disabled = true;
            const list = document.getElementById('wifi-list');
            list.innerHTML = '<div style="padding: 0.75rem 1rem; font-size: 0.85rem; color: var(--text-muted); text-align: center;">正在搜索附近 WiFi 热点...</div>';
            list.classList.add('show');
            
            try {
                const res = await fetch('/api/scan');
                const data = await res.json();
                list.innerHTML = '';
                if (data.networks && data.networks.length > 0) {
                    data.networks.forEach(net => {
                        if (!net.ssid) return;
                        const item = document.createElement('div');
                        item.className = 'wifi-item';
                        item.innerHTML = `<span>${net.ssid}</span><span class="wifi-signal">${net.rssi} dBm</span>`;
                        item.onclick = () => {
                            document.getElementById('ssid').value = net.ssid;
                            list.classList.remove('show');
                        };
                        list.appendChild(item);
                    });
                } else {
                    list.innerHTML = '<div style="padding: 0.75rem 1rem; font-size: 0.85rem; color: var(--text-muted); text-align: center;">未找到可用的 WiFi 网络</div>';
                }
            } catch (err) {
                list.innerHTML = '<div style="padding: 0.75rem 1rem; font-size: 0.85rem; color: #EF4444; text-align: center;">扫描失败</div>';
            } finally {
                btn.textContent = origText;
                btn.disabled = false;
            }
        }

        fetchConfig();
    </script>
</body>
</html>
)rawhtml";

// REST APIs
static void handle_get_wifi() {
    String json = "{\"ssid\":\"" + s_sta_ssid + "\",\"pass\":\"" + s_sta_password + "\"}";
    s_server.send(200, "application/json", json);
}

static void handle_post_wifi() {
    if (s_server.hasArg("ssid") && s_server.hasArg("password")) {
        s_sta_ssid = s_server.arg("ssid");
        s_sta_password = s_server.arg("password");
        s_prefs.putString("sta_ssid", s_sta_ssid);
        s_prefs.putString("sta_pass", s_sta_password);
        Serial.printf("[WebConfig] WiFi configured: SSID: %s\n", s_sta_ssid.c_str());
        s_server.send(200, "text/plain", "OK");
        return;
    }
    s_server.send(400, "text/plain", "Bad Request");
}

static void handle_get_warmup() {
    String json = "{\"warmup\":" + String(s_warmup_sec, 2) + "}";
    s_server.send(200, "application/json", json);
}

static void handle_post_warmup() {
    if (s_server.hasArg("warmup")) {
        float warmup = s_server.arg("warmup").toFloat();
        if (warmup >= 0.1f && warmup <= 10.0f) {
            s_warmup_sec = warmup;
            s_prefs.putFloat("warmup_sec", warmup);
            Serial.printf("[WebConfig] Warm-up time updated to %.2f seconds\n", warmup);
            s_server.send(200, "text/plain", "OK");
            return;
        }
    }
    s_server.send(400, "text/plain", "Bad Request");
}

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

void web_config_init() {
    // 1. 初始化 NVS 存储
    s_prefs.begin("camera_cfg", false);
    s_sta_ssid = s_prefs.getString("sta_ssid", WIFI_SSID);
    s_sta_password = s_prefs.getString("sta_pass", WIFI_PASSWORD);
    s_warmup_sec = s_prefs.getFloat("warmup_sec", 0.8f);

    Serial.printf("[WebConfig] Loaded SSID: %s, Warmup: %.2f sec\n", s_sta_ssid.c_str(), s_warmup_sec);

    // 2. 启动 AP_STA 模式并开启软 AP
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("WaterCamera_AP", "12344321");
    Serial.printf("[WebConfig] SoftAP started. SSID: \"WaterCamera_AP\", IP: %s\n", WiFi.softAPIP().toString().c_str());

    // 3. 注册内置路由
    s_server.on("/", HTTP_GET, []() {
        s_server.send_P(200, "text/html", INDEX_HTML);
    });
    s_server.on("/api/wifi", HTTP_GET, handle_get_wifi);
    s_server.on("/api/wifi", HTTP_POST, handle_post_wifi);
    s_server.on("/api/warmup", HTTP_GET, handle_get_warmup);
    s_server.on("/api/warmup", HTTP_POST, handle_post_warmup);
    s_server.on("/api/scan", HTTP_GET, handle_wifi_scan);

    s_server.begin();
    Serial.println("[WebConfig] Embedded Web Server started on port 80");
}

void web_config_loop() {
    // 防回退机制
    if (WiFi.getMode() == WIFI_STA) {
        Serial.println("[WebConfig] WiFi mode was reverted to STA. Restoring AP_STA and restarting softAP...");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("WaterCamera_AP", "12344321");
    }
    s_server.handleClient();
}

String get_sta_ssid() {
    return s_sta_ssid;
}

String get_sta_password() {
    return s_sta_password;
}

float get_warmup_sec() {
    return s_warmup_sec;
}
