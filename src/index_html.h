#pragma once

#include <Arduino.h>

// 极简暗黑风格 SPA 多标签前端 HTML（存储于 Flash PROGMEM，不占用 RAM）
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>水质监控相机节点配置</title>
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
            padding: 1.25rem 1rem;
            display: flex;
            justify-content: center;
            align-items: flex-start;
            min-height: 100vh;
        }

        .container {
            width: 100%;
            max-width: 480px;
        }

        header {
            text-align: center;
            margin-bottom: 1.25rem;
        }

        header h1 {
            font-size: 1.4rem;
            font-weight: 700;
            letter-spacing: 0.5px;
            background: linear-gradient(135deg, var(--accent-blue), #06B6D4);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 0.35rem;
        }

        header p {
            font-size: 0.82rem;
            color: var(--text-muted);
        }

        /* 顶部 Tab 导航风格 */
        .nav-tabs {
            display: flex;
            background-color: var(--bg-card);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            padding: 0.3rem;
            margin-bottom: 1.25rem;
            gap: 0.25rem;
        }

        .tab-item {
            flex: 1;
            text-align: center;
            padding: 0.6rem 0.3rem;
            font-size: 0.85rem;
            font-weight: 600;
            color: var(--text-muted);
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s ease;
            user-select: none;
        }

        .tab-item:hover {
            color: var(--text-main);
        }

        .tab-item.active {
            background: linear-gradient(135deg, rgba(56, 189, 248, 0.18), rgba(6, 182, 212, 0.18));
            color: var(--accent-blue);
            border: 1px solid rgba(56, 189, 248, 0.35);
        }

        .tab-content {
            display: none;
        }

        .tab-content.active {
            display: block;
        }

        .card {
            background-color: var(--bg-card);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            padding: 1.25rem;
            margin-bottom: 1.25rem;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3), 0 8px 10px -6px rgba(0, 0, 0, 0.3);
        }

        .card-title {
            font-size: 1rem;
            font-weight: 600;
            color: var(--accent-blue);
            margin-bottom: 1rem;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 0.5rem;
        }

        .form-group {
            display: flex;
            flex-direction: column;
            gap: 0.4rem;
            margin-bottom: 1.1rem;
        }

        label {
            font-size: 0.83rem;
            color: var(--text-muted);
            font-weight: 500;
        }

        input, select {
            background-color: #0B0F19;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            color: var(--text-main);
            padding: 0.65rem 0.8rem;
            font-size: 0.88rem;
            outline: none;
            width: 100%;
            transition: border-color 0.2s;
        }

        input:focus, select:focus {
            border-color: var(--accent-blue);
        }

        input[readonly] {
            background-color: rgba(15, 23, 42, 0.6);
            color: var(--accent-blue);
            cursor: not-allowed;
            border-style: dashed;
        }

        .btn {
            background: linear-gradient(135deg, var(--accent-blue), #06B6D4);
            color: #FFFFFF;
            border: none;
            border-radius: 6px;
            padding: 0.75rem 1rem;
            font-size: 0.88rem;
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
            font-size: 0.88rem;
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
            font-size: 0.85rem;
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
            font-size: 0.78rem;
            color: var(--text-muted);
            margin-top: 1.5rem;
            border-top: 1px solid var(--border-color);
            padding-top: 1rem;
        }
    </style>
</head>
<body>

    <div class="container">
        <header>
            <h1>智能相机节点配置</h1>
            <p>AP+STA 双模在线管理控制面板</p>
        </header>

        <!-- 顶部导航 Tabs -->
        <nav class="nav-tabs">
            <div class="tab-item active" onclick="switchTab(0)">网络与系统</div>
            <div class="tab-item" onclick="switchTab(1)">相机参数</div>
            <div class="tab-item" onclick="switchTab(2)">报警与调试</div>
        </nav>

        <!-- PAGE 1: 网络与系统配置 -->
        <div id="tab-0" class="tab-content active">
            <div class="card">
                <div class="card-title">网络与 MQTT 基础配置</div>
                <form id="wifi-form" onsubmit="saveWifi(event)">
                    <div class="form-group">
                        <label for="ssid">Wi-Fi 网络名称 (SSID)</label>
                        <div style="display: flex; gap: 0.5rem;">
                            <input type="text" id="ssid" name="ssid" placeholder="输入外部 Wi-Fi SSID" style="flex: 1;" required>
                            <button type="button" class="btn" style="width: auto; padding: 0.5rem 1rem; font-size: 0.82rem;" onclick="scanWifi(this)">扫描</button>
                        </div>
                        <div id="wifi-list" class="wifi-list"></div>
                    </div>
                    <div class="form-group">
                        <label for="password">Wi-Fi 网络密码 (Password)</label>
                        <input type="password" id="password" name="password" placeholder="输入外部 Wi-Fi 密码">
                    </div>

                    <div style="margin: 1.25rem 0 1rem 0; border-top: 1px solid var(--border-color); padding-top: 0.8rem;">
                        <h4 style="font-size: 0.9rem; font-weight: 600; color: var(--accent-blue); margin-bottom: 0.75rem;">站点与 Broker 参数</h4>
                    </div>

                    <div class="form-group">
                        <label for="name">站点标识 (STATION_NAME)</label>
                        <input type="text" id="name" name="name" placeholder="例如: home" required>
                        <span style="font-size: 0.72rem; color: var(--text-muted);">唯一标识，作为 MQTT 拍照指令响应与报警 Topic 的后缀后缀名称</span>
                    </div>
                    <div class="form-group">
                        <label for="broker">MQTT Broker 地址</label>
                        <input type="text" id="broker" name="broker" placeholder="例如: voicevon.vicp.io" required>
                    </div>
                    <div class="form-group">
                        <label for="port">MQTT 端口 (Port)</label>
                        <input type="number" id="port" name="port" min="1" max="65535" placeholder="默认: 1883" required>
                    </div>

                    <button type="submit" class="btn">保存网络与系统配置</button>
                </form>
            </div>
        </div>

        <!-- PAGE 2: 相机参数配置 -->
        <div id="tab-1" class="tab-content">
            <div class="card">
                <div class="card-title">相机拍照与控制参数</div>
                <form id="warmup-form" onsubmit="saveWarmup(event)">
                    <div class="form-group">
                        <label for="warmup">闪光灯预热延迟 (Warm-up Time)</label>
                        <div style="display: flex; gap: 0.5rem; align-items: center;">
                            <input type="number" id="warmup" name="warmup" step="0.1" min="0.1" max="10.0" placeholder="0.8" style="flex: 1;" required>
                            <span style="color: var(--text-muted); font-size: 0.9rem;">秒</span>
                        </div>
                        <span style="font-size: 0.72rem; color: var(--text-muted); margin-top: 0.25rem; display: block;">亮灯至快门开启的时间间隔（如 0.8 秒），用于自动白平衡 (AEC) 与曝光 (AGC) 充分稳定</span>
                    </div>
                    <div class="form-group">
                        <label for="bright_thresh">最低环境亮度阈值 (Brightness Threshold)</label>
                        <input type="number" id="bright_thresh" name="bright_thresh" min="0" max="255" placeholder="80" required>
                        <span style="font-size: 0.72rem; color: var(--text-muted); margin-top: 0.25rem; display: block;">图像平均灰度亮度阈值 (0–255，默认 80)。低于此值时，下一次拍照将自动开启闪光灯补光</span>
                    </div>
                    <button type="submit" class="btn">保存相机参数</button>
                </form>
            </div>
        </div>

        <!-- PAGE 3: 自动报警与调试 -->
        <div id="tab-2" class="tab-content">
            <!-- 报警参数设置卡片 -->
            <div class="card">
                <div class="card-title">图像突变自动报警配置</div>
                <form id="mutation-form" onsubmit="saveMutation(event)">
                    <div class="form-group">
                        <label for="mutation_enable">自动报警开关</label>
                        <select id="mutation_enable" name="enable">
                            <option value="1">开启 (Enable)</option>
                            <option value="0">关闭 (Disable)</option>
                        </select>
                    </div>

                    <div class="form-group">
                        <label for="mutation_interval">背景检测周期 (Interval)</label>
                        <div style="display: flex; gap: 0.5rem; align-items: center;">
                            <input type="number" id="mutation_interval" name="interval" min="1" max="3600" placeholder="10" style="flex: 1;" required>
                            <span style="color: var(--text-muted); font-size: 0.9rem;">秒</span>
                        </div>
                        <span style="font-size: 0.72rem; color: var(--text-muted);">自动抓拍并评估水质背景帧的时间间隔</span>
                    </div>

                    <div class="form-group">
                        <label for="mutation_thresh">敏感度阈值 (Threshold)</label>
                        <input type="number" id="mutation_thresh" name="thresh" step="0.01" min="0.01" max="2.00" placeholder="0.20" required>
                        <span style="font-size: 0.72rem; color: var(--text-muted);">单网格 RGB 偏差率阈值（越小越敏感，默认 0.20）</span>
                    </div>

                    <div class="form-group">
                        <label for="mutation_min_blocks">最小触发网格数 (Min Blocks)</label>
                        <input type="number" id="mutation_min_blocks" name="min_blocks" min="1" max="64" placeholder="4" required>
                        <span style="font-size: 0.72rem; color: var(--text-muted);">判定为局部突变报警的最小变化网格数量 (1–64)</span>
                    </div>

                    <div class="form-group">
                        <label>当前 MQTT 报警 Topic 路径 (自动动态生成)</label>
                        <input type="text" id="alarm_topic_display" readonly value="water/photo/mutation_alarm/home">
                        <span style="font-size: 0.72rem; color: var(--text-muted);">自动关联站点标识拼接，多节点主题互不冲突</span>
                    </div>

                    <button type="submit" class="btn">保存报警配置</button>
                </form>
            </div>

            <!-- 实时调试与状态卡片 -->
            <div class="card" style="border-color: rgba(56, 189, 248, 0.3);">
                <div class="card-title" style="display: flex; justify-content: space-between; align-items: center;">
                    <span>算法调试与状态监控</span>
                    <span id="debug-status-badge" style="font-size: 0.72rem; padding: 0.2rem 0.5rem; border-radius: 9999px; background: rgba(148, 163, 184, 0.2); color: var(--text-muted);">未更新</span>
                </div>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 0.75rem; margin-bottom: 0.75rem;">
                    <div style="background: rgba(0, 0, 0, 0.25); padding: 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); text-align: center;">
                        <div style="font-size: 0.75rem; color: var(--text-muted); margin-bottom: 0.25rem;">全局帧亮度 (Y)</div>
                        <div id="stat-y" style="font-size: 1.3rem; font-weight: 700; color: var(--accent-blue);">--</div>
                    </div>
                    <div style="background: rgba(0, 0, 0, 0.25); padding: 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); text-align: center;">
                        <div style="font-size: 0.75rem; color: var(--text-muted); margin-bottom: 0.25rem;">突变网格数 (C)</div>
                        <div id="stat-c" style="font-size: 1.3rem; font-weight: 700; color: var(--text-main);">-- / 64</div>
                    </div>
                </div>
                <div style="background: rgba(0, 0, 0, 0.25); padding: 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); display: flex; justify-content: space-between; align-items: center;">
                    <span style="font-size: 0.82rem; color: var(--text-muted);">最新判定结果:</span>
                    <span id="stat-alarm" style="font-size: 0.88rem; font-weight: 700; color: var(--text-muted);">监听中...</span>
                </div>
            </div>
        </div>

        <div class="footer">
            <p>设备型号: Water Camera Node (ESP32-CAM)</p>
            <p>版本信息: Version 2.0 Multi-Tab (2026年8月)</p>
            <p>版权所有 © 山东卷烟厂 &amp; 技术支持：山东卷积分公司</p>
        </div>
    </div>

    <div id="toast">保存成功，已即时生效！</div>

    <script>
        let statusInterval = null;

        function switchTab(index) {
            const tabs = document.querySelectorAll('.tab-item');
            const contents = document.querySelectorAll('.tab-content');
            tabs.forEach((t, i) => {
                t.classList.toggle('active', i === index);
                contents[i].classList.toggle('active', i === index);
            });

            if (index === 2) {
                fetchMutationConfig();
                fetchMutationStatus();
                if (!statusInterval) {
                    statusInterval = setInterval(fetchMutationStatus, 2000);
                }
            } else {
                if (statusInterval) {
                    clearInterval(statusInterval);
                    statusInterval = null;
                }
            }
        }

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

                // 更新动态报警 Topic 路径
                if (data.name) {
                    document.getElementById('alarm_topic_display').value = 'water/photo/mutation_alarm/' + data.name;
                }
            } catch (err) {
                console.error("Fetch config failed:", err);
            }

            try {
                const res = await fetch('/api/warmup');
                const data = await res.json();
                document.getElementById('warmup').value = data.warmup !== undefined ? data.warmup : 0.8;
                document.getElementById('bright_thresh').value = data.bright_thresh !== undefined ? data.bright_thresh : 80;
            } catch (err) {
                console.error("Fetch warmup failed:", err);
            }
        }

        async function fetchMutationConfig() {
            try {
                const res = await fetch('/api/mutation');
                const data = await res.json();
                document.getElementById('mutation_enable').value = data.enable ? "1" : "0";
                document.getElementById('mutation_interval').value = data.interval !== undefined ? data.interval : 10;
                document.getElementById('mutation_thresh').value = data.thresh !== undefined ? data.thresh : 0.20;
                document.getElementById('mutation_min_blocks').value = data.min_blocks !== undefined ? data.min_blocks : 4;
                if (data.alarm_topic) {
                    document.getElementById('alarm_topic_display').value = data.alarm_topic;
                }
            } catch (err) {
                console.error("Fetch mutation config failed:", err);
            }
        }

        async function fetchMutationStatus() {
            try {
                const res = await fetch('/api/mutation/status');
                const data = await res.json();
                document.getElementById('stat-y').textContent = data.y_global !== undefined ? data.y_global : '--';
                document.getElementById('stat-c').textContent = (data.c_changed !== undefined ? data.c_changed : '--') + ' / 64';
                
                const alarmBadge = document.getElementById('stat-alarm');
                if (data.alarm) {
                    alarmBadge.textContent = '🚨 触发突变报警';
                    alarmBadge.style.color = '#EF4444';
                } else {
                    alarmBadge.textContent = '🟢 正常 (未触发)';
                    alarmBadge.style.color = '#10B981';
                }

                const statusBadge = document.getElementById('debug-status-badge');
                if (data.sec_ago !== undefined && data.sec_ago < 9999) {
                    statusBadge.textContent = data.sec_ago + '秒前更新';
                } else {
                    statusBadge.textContent = '无帧数据';
                }

                if (data.alarm_topic) {
                    document.getElementById('alarm_topic_display').value = data.alarm_topic;
                }
            } catch (err) {
                console.error("Fetch status failed:", err);
            }
        }

        async function saveWifi(e) {
            e.preventDefault();
            const form = document.getElementById('wifi-form');
            const params = new URLSearchParams(new FormData(form));
            try {
                const res = await fetch('/api/sysconfig', { method: 'POST', body: params });
                if (res.ok) {
                    showToast("网络配置已保存，已即时生效！");
                    const newName = document.getElementById('name').value;
                    if (newName) {
                        document.getElementById('alarm_topic_display').value = 'water/photo/mutation_alarm/' + newName;
                    }
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

        async function saveMutation(e) {
            e.preventDefault();
            const form = document.getElementById('mutation-form');
            const params = new URLSearchParams(new FormData(form));
            try {
                const res = await fetch('/api/mutation', { method: 'POST', body: params });
                if (res.ok) {
                    showToast("突变报警配置已保存，立即生效！");
                } else {
                    alert("保存失败");
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
