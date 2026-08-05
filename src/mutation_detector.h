#pragma once

#include <Arduino.h>
#include "esp_camera.h"

// ============================================================
//  常量定义
// ============================================================
#define MD_GRID_W        8     // 水平方向网格数
#define MD_GRID_H        8     // 垂直方向网格数
#define MD_GRID_COUNT    64    // 总网格数 (8×8)
#define MD_WINDOW_SIZE   20    // 滑动窗口容量（帧数）

// ============================================================
//  MutationDetector 类
// ============================================================
class MutationDetector {
public:
    MutationDetector();

    /**
     * @brief 处理一帧背景评估图像，执行突变检测
     *
     * 调用者需确保：
     *  - fb 非空，格式为 PIXFORMAT_JPEG
     *  - 调用前已关闭闪光灯（背景帧）
     *  - 调用完成后由调用者负责 camera.release(fb)
     *
     * @param fb  来自 camera.capture() 的帧缓冲指针
     * @return    true = 本帧判定为局部突变并已发送 MQTT 报警
     *            false = 无突变、全局光变或预热期
     */
    bool processFrame(camera_fb_t* fb);

    // 调试与状态查询接口
    float    getLastYGlobal() const { return _last_y_global; }
    int      getLastCChanged() const { return _last_c_changed; }
    bool     getLastAlarmStatus() const { return _last_alarm; }
    uint32_t getLastUpdateMs() const { return _last_update_ms; }
    uint32_t getLastAlarmMs() const { return _last_alarm_ms; }

private:
    // 最近一帧的调试监控数据
    float    _last_y_global;
    int      _last_c_changed;
    bool     _last_alarm;
    uint32_t _last_update_ms;
    uint32_t _last_alarm_ms;

    // 滑动窗口：存储历史帧各网格的归一化 RGB 特征（3 通道）
    float _window[MD_WINDOW_SIZE][MD_GRID_COUNT][3];

    // 环形缓冲写指针与已积累帧计数
    int   _write_idx;
    int   _frame_count;

    // 各网格移动均值（预计算，随窗口更新同步刷新）
    float _mu[MD_GRID_COUNT][3];

    /**
     * @brief 从 JPEG fb 解码并降采样，填充 8×8 RGB888 网格均值数组
     *
     * @param fb        相机帧
     * @param grid_rgb  输出缓冲，大小 MD_GRID_COUNT × 3（RGB 各通道均值，0–255 浮点）
     * @return          true = 解码成功
     */
    bool _decodeGrid(camera_fb_t* fb, float grid_rgb[MD_GRID_COUNT][3]);

    /**
     * @brief 将新帧归一化特征存入滑动窗口，并重新计算 _mu
     *
     * @param norm_rgb  新帧各网格归一化特征，大小 MD_GRID_COUNT × 3
     */
    void _updateWindow(float norm_rgb[MD_GRID_COUNT][3]);
};

// 全局单例声明
extern MutationDetector mutationDetector;
