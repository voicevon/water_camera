# 相机画面突变检测设计文档 (Mutation Detection Design)

## 1. 概述与设计背景 (Overview)

在 `water_camera` 项目中，现有的拍照机制分为两种：
1. **主动/指令拍照**：收到 MQTT 命令（或自检）触发，开启闪光灯（若需要）拍摄高质 JPEG 并上报给 MQTT/服务器。
2. **背景评估拍照**：在设备空闲时定期触发（原代码在空闲 60 秒触发一次），关闭闪光灯拍摄一帧图片。**原代码中该背景照片仅用于 `evaluate_brightness()` 计算曝光与环境亮度**，随后即被释放，利用率较低。

本项目旨在**复用现有的背景评估拍照机制**，并联动 **Web Config (Web 配置网页) 与 NVS 存储**：
- **完全不增加额外的硬件拍照开销**，直接将背景评估得到的帧数据 `fb` 传递给新增的 `MutationDetector` 模块。
- **Web Config 实时联动**：用户可以通过网页动态修改突变检测使能状态、检测间隔、灵敏度阈值和触发网格数，**修改后保存立刻写入 NVS 并实时生效（无需重启）**。

---

## 2. 系统架构与数据流 (Architecture & Data Flow)

```
       ┌───────────────────────────────┐
       │ Web Config 界面 / API         │
       │ (网页修改参数 -> NVS -> 内存) │
       └───────────────┬───────────────┘
                       │ 实时生效控制
                       ▼
         ┌───────────────────────────┐
         │ 背景拍照定时器触发        │
         │ (间隔由 WebConfig 实时控制)│
         └─────────────┬─────────────┘
                       │
                       ▼
         ┌───────────────────────────┐
         │ camera.capture()          │
         │ (无闪光灯背景评估帧)      │
         └─────────────┬─────────────┘
                       │ (复用同一帧 camera_fb_t* fb)
       ┌───────────────┴───────────────┐
       ▼                               ▼
 ┌───────────────────────────┐   ┌───────────────────────────┐
 │ 逻辑 A: 亮度评估          │   │ 逻辑 B: 突变检测 (新增)   │
 │ evaluate_brightness(fb)   │   │ MutationDetector          │
 │ (判断下一次是否开启闪光灯)│   │ .processFrame(fb)         │
 └───────────────────────────┘   └─────────────┬─────────────┘
                                               │
                                               ▼
                                 ┌───────────────────────────┐
                                 │ 画面突变判定              │
                                 │ (局部异物/水质突变/报警)  │
                                 └───────────────────────────┘
```

---

## 3. Web Config 可配置参数列表 (Configurable Parameters)

用户可在网页 Web 界面上动态配置并持久化以下参数：

| 参数名（代码/API） | NVS 键名 | 数据类型 | 默认值 | 可调范围 | 说明 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `mutation_enable` | `mut_enable` | `bool` | `true` | 开 / 关 | 是否开启画面突变检测功能 |
| `mutation_interval_sec` | `mut_interval` | `int` | `10` | $5 \sim 300$ 秒 | 背景拍照兼突变检测采样频率（秒） |
| `mutation_block_thresh` | `mut_thresh` | `float` | `0.15` | $0.05 \sim 0.50$ ($5\%\sim50\%$) | 单网格偏离基线的判定阈值 |
| `mutation_min_blocks` | `mut_min_blk` | `int` | `3` | $1 \sim 32$ | 触发突变报警的最少网格数 |

> **⚠️ NVS 键名限制**：ESP32 NVS 键名最长 **15 字节**。上表 NVS 键名列已按此限制缩写，实现时 `s_prefs.putXxx()` 调用必须使用缩写键名，而非参数名全称。

---

## 4. 突变检测核心算法 (Mutation Detection Algorithm)

### 4.0 JPEG 解码前置步骤（实现必读）

相机驱动配置为 `PIXFORMAT_JPEG`，`camera_fb_t::buf` 为压缩 JPEG 数据，**无法直接访问像素**，必须先解码。

**采用方案：`esp_jpg_decode` 降采样解码**
- 使用 ESP-IDF 内置的 `esp_jpg_decode()`（已随 esp-camera 组件引入，零额外依赖）。
- 解码目标：直接输出到 $8 \times 8$ 的 RGB888 小缓冲区（`uint8_t grid_buf[8][8][3]`，仅 **192 字节**），绕过全帧像素解码的内存开销。
- 实现方式：向解码器注册自定义输出回调，在回调中按源图尺寸计算采样步长，仅写入落在网格中心附近的像素，其余丢弃。

```
解码流程示意：
JPEG (fb->buf) → esp_jpg_decode() → 8×8 RGB888 grid_buf[192 B]
                                          ↓
                                   计算各网格均值 B_i
```

> **CPU 耗时补充说明**：含 JPEG 解码步骤后，实际处理耗时约为 $50 \sim 150\,\text{ms}$（视分辨率和解码实现而定），远高于第 6 节原始估算，详见第 6 节修正。

### 4.1 降采样与网格划分

- 将 JPEG 背景帧经 4.0 节解码后，得到 $8 \times 8 = 64$ 个小网格的 RGB 均值。
- 计算各网格 RGB 通道平均值 $B_i = (\bar{R}_i, \bar{G}_i, \bar{B}_i)$ 及全图总亮度 $Y_{global} = \frac{1}{64}\sum_i (\bar{R}_i + \bar{G}_i + \bar{B}_i)$。

### 4.2 全局光照归一化 (Illumination Normalization)

- 计算各网格归一化特征：$N_i = B_i / Y_{global}$。
- 日光渐变或云彩遮挡时，$B_i$ 与 $Y_{global}$ 同步起伏，归一化特征 $N_i$ 保持恒定，有效排除全局光照误报。
- **零值保护**：当 $Y_{global} < \epsilon$（建议 $\epsilon = 1.0$，对应极暗或镜头遮挡场景）时，直接跳过本帧检测，只将新帧纳入基线更新，避免除零产生 NaN：
  ```cpp
  if (y_global < 1.0f) {
      update_baseline_only(n_new); // 更新基线，不报警
      return;
  }
  ```

### 4.3 滑动窗口基线维护 (Sliding Window)

- 维护容量 $N_{window} = 20$ 帧的环形缓冲区，保存历史归一化特征。
- 动态计算各网格移动平均值 $\mu_i$。
- **预热期**：前 $N_{window}$ 帧（即 20 帧）内仅积累基线，不执行报警判定，避免初始基线不稳定时产生误报。

### 4.4 突变与报警判定 (Mutation & Alarm)

- 计算新帧各网格偏差率：$\Delta_i = |N_i^{new} - \mu_i| / \mu_i$。
- 统计偏差率超过网页设定阈值（如 $\Delta_i > \text{mutation\_block\_thresh}$）的网格数量 $C_{changed}$。
- 若 $\text{mutation\_min\_blocks} \le C_{changed} \le 25$，判定为**局部突变（异物进入/水质异常）**，向 MQTT 发送突变报警 `water/photo/mutation_alarm`。
- 若 $C_{changed} > 25$，判定为**全局环境光骤变**，更新基线而不误报。

> **注**：阈值 $C_{changed} > 25$（约 $39\%$ 的网格数）为经验常量，依据是全局光照骤变通常影响画面大部分区域。此值在初版中固定，后续可根据实际场景数据调整。

---

## 5. 接口与 Web API 规范 (Web API & NVS Integration)

### 5.1 REST API 接口
- `GET /api/mutation`
  - 返回当前突变检测配置 JSON：
    ```json
    {
      "enable": true,
      "interval": 10,
      "thresh": 0.15,
      "min_blocks": 3
    }
    ```
- `POST /api/mutation`
  - 接收表单或 JSON 参数，更新 NVS 并在内存中实时生效。

### 5.2 NVS 持久化实现
在 `nvs_config.h` / `nvs_config.cpp` 中新增扩展 Setter/Getter，照现有 `warmup_sec` 的 `begin/putXxx/end` 样板实现：

| 函数签名 | NVS 键名 | 存储类型 |
| :--- | :--- | :--- |
| `get_mutation_enable()` / `nvs_set_mutation_enable(bool)` | `mut_enable` | `Bool` |
| `get_mutation_interval_sec()` / `nvs_set_mutation_interval_sec(int)` | `mut_interval` | `Int` |
| `get_mutation_block_thresh()` / `nvs_set_mutation_block_thresh(float)` | `mut_thresh` | `Float` |
| `get_mutation_min_blocks()` / `nvs_set_mutation_min_blocks(int)` | `mut_min_blk` | `Int` |

> **注**：以上 NVS 键名均已控制在 15 字节以内，满足 ESP32 NVS 硬性限制。

### 5.3 main.cpp 修改要点

现有 `main.cpp` 中空闲评估触发间隔为**硬编码 60 秒**：

```cpp
// 修改前（main.cpp L245）
if (now - s_last_photo_time_ms >= 60000UL) {
```

实现时必须替换为可配置值，否则 Web 界面修改间隔参数不会生效：

```cpp
// 修改后
unsigned long interval_ms = (unsigned long)get_mutation_interval_sec() * 1000UL;
if (now - s_last_photo_time_ms >= interval_ms) {
```

---

## 6. 性能与资源开销 (Resource Overhead)

| 开销项 | 估算 | 说明 |
| :--- | :--- | :--- |
| 滑动窗口内存 | $20 \times 64 \times 4\,\text{B} \approx 5.1\,\text{KB}$ | 对 ESP32-CAM（4 MB PSRAM）极其轻量 |
| 解码缓冲区 | $8 \times 8 \times 3 = 192\,\text{B}$ | `grid_buf` 栈或静态分配均可 |
| JPEG 解码耗时 | $50 \sim 150\,\text{ms}$ | 使用 `esp_jpg_decode` 降采样，耗时随分辨率线性缩放 |
| 均值/归一化计算 | $< 1\,\text{ms}$ | 64 个网格的浮点运算，微不足道 |
| **总 CPU 耗时** | **$\approx 50 \sim 150\,\text{ms}$** | 在背景空闲拍照后串行执行，不阻塞主循环的 MQTT/WiFi 处理 |

> 背景评估拍照本身由 `millis()` 计时驱动（非抢占），$50 \sim 150\,\text{ms}$ 的处理窗口在两次 `loop()` 调用间完成，对系统实时性无影响。

---

## 7. 已知局限与设计决策 (Known Limitations)

| 局限 | 说明 | 处置方式 |
| :--- | :--- | :--- |
| **渐进式污染检测盲区** | 水质缓慢变化时（如微生物逐渐繁殖），每帧偏差均低于阈值，但基线随之漂移，导致长期变化无法被检测 | 可在未来版本引入长期基线（低更新速率的第二层滑动窗口）对比短期基线；初版标注为已知局限 |
| **预热期不报警** | 系统启动后需积累 $N_{window} = 20$ 帧才开始判定，期间不发出报警 | 预热期内在日志中打印 `[Mutation] Warming up (N/20)`，告知用户 |
| **全局骤变阈值为经验常量** | $C_{changed} > 25$（39% 网格）的全局/局部分界为经验值 | 初版固定，后续可根据实测场景数据调整 |
