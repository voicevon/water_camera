#include "mutation_detector.h"
#include "nvs_config.h"
#include "web_config.h"
#include "network_handler.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

// esp_jpg_decode: 随 esp32-camera 组件引入，零额外依赖
// API: esp_err_t esp_jpg_decode(size_t len, jpg_scale_t scale,
//                               jpg_reader_cb reader, jpg_writer_cb writer, void* arg)
// reader 和 writer 共享同一个 arg 指针
#include "esp_jpg_decode.h"

// ============================================================
//  解码上下文（reader + writer 共享，通过单一 void* arg 传递）
// ============================================================
struct DecodeArg {
    // --- reader 使用 ---
    const uint8_t* jpeg_data;  // JPEG 原始数据指针
    size_t         jpeg_len;   // JPEG 数据长度

    // --- writer 使用（解码后像素按网格累加）---
    float grid_r[MD_GRID_COUNT]; // 各网格 R 通道累加值
    float grid_g[MD_GRID_COUNT]; // 各网格 G 通道累加值
    float grid_b[MD_GRID_COUNT]; // 各网格 B 通道累加值
    int   pix_cnt[MD_GRID_COUNT]; // 各网格已积累像素数

    // 固定解码尺寸：在 _decodeGrid 初始化时锁定，不在 writer 中修改
    // 用于网格坐标计算，确保所有像素块使用相同的分母
    int img_w;  // JPG_SCALE_8X 后预估宽度 = fb->width  / 8
    int img_h;  // JPG_SCALE_8X 后预估高度 = fb->height / 8
};

// jpg_reader_cb：向解码器提供 JPEG 原始数据
static size_t _jpeg_reader(void* arg, size_t index, uint8_t* buf, size_t len) {
    DecodeArg* d = (DecodeArg*)arg;
    if (index >= d->jpeg_len) return 0;
    size_t avail = d->jpeg_len - index;
    size_t n = (len < avail) ? len : avail;
    if (buf) {
        memcpy(buf, d->jpeg_data + index, n);
    }
    return n;
}

// jpg_writer_cb：接收解码后一块像素（RGB888），按 8×8 网格累加
// x, y: 块左上角坐标；w, h: 块尺寸；data: RGB888 像素数据（NULL = 结束哨兵）
static bool _jpeg_writer(void* arg, uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h, uint8_t* data) {
    if (!data) return true; // 解码结束的哨兵调用，直接返回

    DecodeArg* d = (DecodeArg*)arg;

    // 将块内每个像素映射到对应的 8×8 网格并累加 RGB
    // img_w/img_h 在 _decodeGrid 初始化时已锁定（fb->width/8, fb->height/8）
    // 所有像素块都使用相同的分母，网格坐标计算结果一致且正确
    for (uint16_t py = y; py < (uint16_t)(y + h); py++) {
        for (uint16_t px = x; px < (uint16_t)(x + w); px++) {
            int gx = (d->img_w > 0) ? (int)((long)px * MD_GRID_W / d->img_w) : 0;
            int gy = (d->img_h > 0) ? (int)((long)py * MD_GRID_H / d->img_h) : 0;
            if (gx >= MD_GRID_W) gx = MD_GRID_W - 1;
            if (gy >= MD_GRID_H) gy = MD_GRID_H - 1;

            int idx = gy * MD_GRID_W + gx;
            int pix_off = ((int)(py - y) * (int)w + (int)(px - x)) * 3;
            d->grid_r[idx] += (float)data[pix_off];
            d->grid_g[idx] += (float)data[pix_off + 1];
            d->grid_b[idx] += (float)data[pix_off + 2];
            d->pix_cnt[idx]++;
        }
    }
    return true;
}

// ============================================================
//  全局单例
// ============================================================
MutationDetector mutationDetector;

// ============================================================
//  构造函数
// ============================================================
MutationDetector::MutationDetector()
    : _last_y_global(0.0f), _last_c_changed(0), _last_alarm(false),
      _last_update_ms(0), _last_alarm_ms(0),
      _cooldown_sec(DEFAULT_MUTATION_COOLDOWN_SEC), _cooldown_until_ms(0),
      _frame_count(0) {
    memset(_mu, 0, sizeof(_mu));
}

// ============================================================
//  冷却保护状态检查
// ============================================================
bool MutationDetector::isInCooldown() const {
    if (_cooldown_until_ms == 0) return false;
    return millis() < _cooldown_until_ms;
}

uint32_t MutationDetector::getCooldownRemainingSec() const {
    uint32_t now = millis();
    if (now >= _cooldown_until_ms) return 0;
    return (_cooldown_until_ms - now + 999) / 1000;
}

// ============================================================
//  私有：JPEG 解码并降采样至 8×8 网格 RGB 均值
// ============================================================
bool MutationDetector::_decodeGrid(camera_fb_t* fb,
                                   float grid_rgb[MD_GRID_COUNT][3]) {
    static DecodeArg darg;
    memset(&darg, 0, sizeof(darg));
    darg.jpeg_data = fb->buf;
    darg.jpeg_len  = fb->len;
    darg.img_w = (int)fb->width  / 8;  // VGA(640x480) -> 80
    darg.img_h = (int)fb->height / 8;  // VGA(640x480) -> 60

    esp_err_t err = esp_jpg_decode(fb->len, JPG_SCALE_8X,
                                   _jpeg_reader, _jpeg_writer, &darg);
    if (err != ESP_OK) {
        Serial.printf("[Mutation] esp_jpg_decode failed: 0x%x\n", err);
        return false;
    }

    for (int i = 0; i < MD_GRID_COUNT; i++) {
        if (darg.pix_cnt[i] > 0) {
            grid_rgb[i][0] = darg.grid_r[i] / (float)darg.pix_cnt[i];
            grid_rgb[i][1] = darg.grid_g[i] / (float)darg.pix_cnt[i];
            grid_rgb[i][2] = darg.grid_b[i] / (float)darg.pix_cnt[i];
        } else {
            grid_rgb[i][0] = grid_rgb[i][1] = grid_rgb[i][2] = 0.0f;
        }
    }
    return true;
}

// ============================================================
//  私有：EMA 更新基准均值
// ============================================================
void MutationDetector::_updateEMA(float norm_rgb[MD_GRID_COUNT][3], float alpha) {
    if (alpha <= 0.0f) return;
    if (alpha >= 1.0f) {
        memcpy(_mu, norm_rgb, sizeof(_mu));
        return;
    }
    float one_minus_alpha = 1.0f - alpha;
    for (int i = 0; i < MD_GRID_COUNT; i++) {
        _mu[i][0] = one_minus_alpha * _mu[i][0] + alpha * norm_rgb[i][0];
        _mu[i][1] = one_minus_alpha * _mu[i][1] + alpha * norm_rgb[i][1];
        _mu[i][2] = one_minus_alpha * _mu[i][2] + alpha * norm_rgb[i][2];
    }
}

// ============================================================
//  私有：报警后快速吸纳当前帧为基准
// ============================================================
void MutationDetector::_fastAdapt(float norm_rgb[MD_GRID_COUNT][3]) {
    // 将当前帧直接设为新基准，使静止异物迅速收编为背景，彻底杜绝静止重复报警
    memcpy(_mu, norm_rgb, sizeof(_mu));
}

// ============================================================
//  主接口：处理一帧，返回是否触发突变报警
// ============================================================
bool MutationDetector::processFrame(camera_fb_t* fb) {
    // 检查全局使能开关
    if (!get_mutation_enable()) {
        return false;
    }
    if (!fb) return false;

    uint32_t t0 = millis();

    // Step 1：JPEG 解码 → 8×8 网格 RGB 均值
    float grid_rgb[MD_GRID_COUNT][3];
    if (!_decodeGrid(fb, grid_rgb)) {
        return false;
    }

    // Step 2：计算全图平均总亮度 Y_global（三通道均值的全局均值）
    float y_global = 0.0f;
    for (int i = 0; i < MD_GRID_COUNT; i++) {
        y_global += (grid_rgb[i][0] + grid_rgb[i][1] + grid_rgb[i][2]);
    }
    y_global /= (float)(MD_GRID_COUNT * 3); // 单通道全局均值

    // Step 3：全局光照归一化，含零值保护（防止除零 / NaN）
    float norm_rgb[MD_GRID_COUNT][3];
    if (y_global < 1.0f) {
        // 极暗场景（镜头遮挡/纯黑），跳过报警判定
        Serial.printf("[Mutation] Y_global=%.2f < 1.0, too dark. Skipping evaluation.\n",
                      y_global);
        return false;
    }

    for (int i = 0; i < MD_GRID_COUNT; i++) {
        norm_rgb[i][0] = grid_rgb[i][0] / y_global;
        norm_rgb[i][1] = grid_rgb[i][1] / y_global;
        norm_rgb[i][2] = grid_rgb[i][2] / y_global;
    }

    // Step 4：预热期检查（前 MUTATION_WARMUP_FRAMES 帧快速收敛基线，不执行报警）
    if (_frame_count < MUTATION_WARMUP_FRAMES) {
        if (_frame_count == 0) {
            memcpy(_mu, norm_rgb, sizeof(_mu));
        } else {
            _updateEMA(norm_rgb, 0.5f); // 预热期快速加权更新
        }
        _frame_count++;
        Serial.printf("[Mutation] Warming up (%d/%d). Y_global=%.1f\n",
                      _frame_count, MUTATION_WARMUP_FRAMES, y_global);
        return false;
    }

    // Step 5：逐网格计算三通道偏差率均值 Δ_i，统计超阈值网格数
    float thresh     = get_mutation_block_thresh();
    int   min_blocks = get_mutation_min_blocks();
    int   c_changed  = 0;

    for (int i = 0; i < MD_GRID_COUNT; i++) {
        float delta = 0.0f;
        for (int ch = 0; ch < 3; ch++) {
            if (_mu[i][ch] > 1e-6f) {
                delta += fabsf(norm_rgb[i][ch] - _mu[i][ch]) / _mu[i][ch];
            }
        }
        delta /= 3.0f; // 三通道偏差率取均值

        if (delta > thresh) {
            c_changed++;
        }
    }

    // Step 6：检查冷却保护状态
    bool in_cooldown = isInCooldown();
    uint32_t cooldown_rem = getCooldownRemainingSec();

    bool alarm = false;

    if (in_cooldown) {
        // 冷却期内：抑制重复报警和拍照，但持续以平缓 EMA 跟踪背景变化
        _updateEMA(norm_rgb, MUTATION_EMA_ALPHA);
        Serial.printf("[Mutation] In Cooldown (%us remaining). Suppressing alarm (C_changed=%d/%d, thresh=%.2f)\n",
                      cooldown_rem, c_changed, MD_GRID_COUNT, thresh);
    } else {
        // Step 7：突变判定
        if (c_changed >= min_blocks) {
            alarm = true;
            Serial.printf("[Mutation] ALARM! Mutation detected: C_changed=%d >= min_blocks=%d (thresh=%.2f)\n",
                          c_changed, min_blocks, thresh);

            char payload[80];
            snprintf(payload, sizeof(payload),
                     "{\"changed\":%d,\"total\":%d,\"thresh\":%.2f}",
                     c_changed, MD_GRID_COUNT, thresh);
            String alarm_topic = String(MQTT_ALARM_TOPIC) + "/" + get_station_name();
            network.publishText(alarm_topic.c_str(), payload);

            // 1. 触发报警后启动冷却保护期
            _cooldown_until_ms = millis() + (_cooldown_sec * 1000UL);
            Serial.printf("[Mutation] Cooldown timer started: %us\n", _cooldown_sec);

            // 2. 快速吸纳：将当前新画面（包含该异物）更新为基准，消除后续静止时的重复误报
            _fastAdapt(norm_rgb);
        } else {
            // 正常平缓更新 EMA 基准
            _updateEMA(norm_rgb, MUTATION_EMA_ALPHA);
        }
    }

    // 更新调试监控数据
    _last_y_global = y_global;
    _last_c_changed = c_changed;
    _last_alarm = alarm;
    _last_update_ms = millis();
    if (alarm) {
        _last_alarm_ms = millis();
    }

    uint32_t elapsed = millis() - t0;
    Serial.printf("[Mutation] Y_global=%.1f, C_changed=%d/%d, thresh=%.2f, alarm=%s, cooldown=%us, elapsed=%ums\n",
                  y_global, c_changed, MD_GRID_COUNT, thresh,
                  alarm ? "YES" : "NO", cooldown_rem, elapsed);

    return alarm;
}
