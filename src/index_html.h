#pragma once

#include <Arduino.h>

// 极简暗黑风格 SPA 前端 HTML（存储于 Flash PROGMEM，不占用 RAM）
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
            max-width: 480px;
        }

        header {
            text-align: center;
            margin-bottom: 2rem;
        }

        header h1 {
            font-size: 1.5rem;
            font-weight: 700;
            letter-spacing: 0.5px;
            background: linear-gradient(135deg, var(--accent-blue), #06B6D4);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 0.5rem;
        }

        header p {
            font-size: 0.88rem;
            color: var(--text-muted);
        }

        .card {
            background-color: var(--bg-card);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            padding: 1.5rem;
            margin-bottom: 1.5rem;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3), 0 8px 10px -6px rgba(0, 0, 0, 0.3);
        }

        .card-title {
            font-size: 1.05rem;
            font-weight: 600;
            color: var(--accent-blue);
            margin-bottom: 1.25rem;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 0.5rem;
        }

        .form-group {
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
            margin-bottom: 1.25rem;
        }

        label {
            font-size: 0.85rem;
            color: var(--text-muted);
            font-weight: 500;
        }

        input {
            background-color: #0B0F19;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            color: var(--text-main);
            padding: 0.65rem 0.8rem;
            font-size: 0.9rem;
            outline: none;
            width: 100%;
            transition: border-color 0.2s;
        }

        input:focus {
            border-color: var(--accent-blue);
        }

        .btn {
            background: linear-gradient(135deg, var(--accent-blue), #06B6D4);
            color: #FFFFFF;
            border: none;
            border-radius: 6px;
            padding: 0.75rem 1rem;
            font-size: 0.9rem;
            font-weight: 600;
            cursor: pointer;
            width: 100%;
            transition: opacity 0.2s;
            box-shadow: 0 4px 12px rgba(56, 189, 248, 0.25);
            display: flex;
            justify-content: center;
            align-items: center;
        }

        .btn:hover {
            opacity: 0.9;
        }

        #toast {
            position: fixed;
            bottom: 2rem;
            left: 50%;
            transform: translateX(-50%) translateY(100px);
            opacity: 0;
            visibility: hidden;
            background-color: #10B981;
            color: #FFFFFF;
            padding: 0.6rem 1.5rem;
            border-radius: 9999px;
            font-weight: 600;
            box-shadow: 0 10px 25px -5px rgba(16, 185, 129, 0.3);
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

        <!-- WiFi 与系统配置 -->
        <div class="card">
            <div class="card-title">系统与网络配置</div>
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

                <div style="margin: 1.5rem 0 1rem 0; border-top: 1px solid var(--border-color); padding-top: 1rem;">
                    <h4 style="font-size: 0.95rem; font-weight: 600; color: var(--accent-blue); margin-bottom: 0.75rem;">相机与 MQTT 参数</h4>
                </div>

                <div class="form-group">
                    <label for="name">站点标识 (STATION_NAME)</label>
                    <input type="text" id="name" name="name" placeholder="例如: home" required>
                </div>
                <div class="form-group">
                    <label for="broker">MQTT Broker 地址</label>
                    <input type="text" id="broker" name="broker" placeholder="例如: voicevon.vicp.io" required>
                </div>
                <div class="form-group">
                    <label for="port">MQTT 端口 (Port)</label>
                    <input type="number" id="port" name="port" min="1" max="65535" placeholder="默认: 1883" required>
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
            <p>版权所有 © 山东卷烟厂 &amp; 技术支持：山东卷积分公司</p>
        </div>
    </div>

    <div id="toast">保存成功，已即时生效！</div>

    <script>
        // 页面初始化时拉取配置数据
        async function fetchConfig() {
            try {
                const res = await fetch('/api/sysconfig');
                const data = await res.json();
                document.getElementById('ssid').value = data.ssid || '';
                document.getElementById('password').value = data.pass || '';
                document.getElementById('name').value = data.name || '';
                document.getElementById('broker').value = data.broker || '';
                document.getElementById('port').value = data.port || 1883;
            } catch (err) {
                console.error("Fetch config failed:", err);
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
                const res = await fetch('/api/sysconfig', { method: 'POST', body: params });
                if (res.ok) {
                    showToast("配置已保存，网络参数已即时生效！");
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
            const t = document.getElementById('toast');
            if (msg) t.textContent = msg;
            t.classList.add('show');
            setTimeout(() => t.classList.remove('show'), 3000);
        }

        async function scanWifi(btn) {
            const orig = btn.textContent;
            btn.textContent = "扫描中...";
            btn.disabled = true;
            const list = document.getElementById('wifi-list');
            list.innerHTML = '<div style="padding: 0.6rem 1rem; font-size: 0.85rem; color: var(--text-muted); text-align: center;">正在搜寻 Wi-Fi...</div>';
            list.classList.add('show');

            let pollInterval;

            async function poll() {
                try {
                    const res = await fetch('/api/scan');
                    const data = await res.json();
                    if (data.status === "scanning") {
                        return; // 还在扫描，等待下一次轮询
                    }
                    
                    clearInterval(pollInterval);
                    list.innerHTML = '';
                    if (data.networks && data.networks.length > 0) {
                        data.networks.forEach(net => {
                            const div = document.createElement('div');
                            div.className = 'wifi-item';
                            div.innerHTML = `
                                <span>${net.ssid}</span>
                                <span class="wifi-signal">${net.rssi} dBm</span>
                            `;
                            div.onclick = () => {
                                document.getElementById('ssid').value = net.ssid;
                                list.classList.remove('show');
                            };
                            list.appendChild(div);
                        });
                    } else {
                        list.innerHTML = '<div style="padding: 0.6rem 1rem; font-size: 0.85rem; color: var(--text-muted); text-align: center;">未发现 Wi-Fi</div>';
                    }
                    btn.textContent = orig;
                    btn.disabled = false;
                } catch (err) {
                    clearInterval(pollInterval);
                    list.innerHTML = '<div style="padding: 0.6rem 1rem; font-size: 0.85rem; color: #EF4444; text-align: center;">扫描失败</div>';
                    btn.textContent = orig;
                    btn.disabled = false;
                }
            }

            try {
                // 启动扫描（加入 refresh=1 强制开始新一轮异步扫描）
                const res = await fetch('/api/scan?refresh=1');
                const data = await res.json();
                if (data.status === "scanning") {
                    pollInterval = setInterval(poll, 1000);
                } else {
                    list.innerHTML = '<div style="padding: 0.6rem 1rem; font-size: 0.85rem; color: #EF4444; text-align: center;">扫描启动失败</div>';
                    btn.textContent = orig;
                    btn.disabled = false;
                }
            } catch (err) {
                list.innerHTML = '<div style="padding: 0.6rem 1rem; font-size: 0.85rem; color: #EF4444; text-align: center;">连接失败</div>';
                btn.textContent = orig;
                btn.disabled = false;
            }
        }

        window.onload = fetchConfig;
    </script>
</body>
</html>
)rawhtml";
