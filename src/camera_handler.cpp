#include "camera_handler.h"
#include <Arduino.h>
#include "config.h"

// 实例化全局单例
CameraHandler camera;

bool CameraHandler::init() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    
    // 直接硬件输出 JPEG，禁用本地水印，以获取最高画质和帧率
    config.pixel_format = PIXFORMAT_JPEG;
    
    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;   // 有 PSRAM 使用 UXGA(1600×1200) 最高分辨率
        config.jpeg_quality = 4;              // 最高 JPEG 编码质量 (0-63，越小越好)
        config.fb_count = 2;                  // 双缓冲，提高帧捕获和传输效率
        Serial.println("[Camera] PSRAM found. Resolution: UXGA (1600x1200), Format: JPEG, fb_count: 2, jpeg_quality: 4");
    } else {
        config.frame_size = FRAMESIZE_SVGA;   // 无 PSRAM 降级使用 SVGA (800x600)
        config.jpeg_quality = 8;              // 较低的编码质量以节省 RAM
        config.fb_count = 1;
        Serial.println("[Camera] No PSRAM. Resolution: SVGA (800x600), Format: JPEG, fb_count: 1, jpeg_quality: 8");
    }

    // 初始化摄像头
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] Init failed, err: 0x%x\n", err);
        return false;
    }
    
    // 调整 OV2640 传感器图像质量参数（最高画质模式）
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 0);     // 亮度: 0 为中性，保留动态范围
        s->set_contrast(s, 2);       // 对比度: +2 最大，提升细节层次感
        s->set_saturation(s, 1);     // 饱和度: +1，色彩更鲜艳自然
        s->set_sharpness(s, 2);      // 锐度: +2，边缘更清晰（部分固件支持）
        s->set_whitebal(s, 1);       // 自动白平衡: 开启
        s->set_awb_gain(s, 1);       // AWB 增益: 开启
        s->set_wb_mode(s, 0);        // 白平衡模式: 0=自动
        s->set_exposure_ctrl(s, 1);  // 自动曝光: 开启
        s->set_aec2(s, 1);           // AEC DSP 增强: 开启
        s->set_ae_level(s, 0);       // 曝光补偿: 0=中性
        s->set_aec_value(s, 300);    // AEC 目标亮度值（0-1200）
        s->set_gain_ctrl(s, 1);      // 自动增益控制: 开启
        s->set_agc_gain(s, 0);       // AGC 初始增益: 0（自动接管）
        s->set_gainceiling(s, (gainceiling_t)4); // 增益上限: 8x (0=2x…5=128x)，提升暗光表现
        s->set_bpc(s, 1);            // 坏点修正: 开启
        s->set_wpc(s, 1);            // 白点修正: 开启
        s->set_raw_gma(s, 1);        // 原始 Gamma 修正: 开启，提升暗部细节
        s->set_lenc(s, 1);           // 镜头畸变补偿: 开启（消除暗角）
        s->set_dcw(s, 1);            // 下采样控制: 开启（高分辨率必须开启）
        s->set_hmirror(s, 0);        // 水平镜像: 关闭
        s->set_vflip(s, 0);          // 垂直翻转: 关闭
        Serial.println("[Camera] Sensor tuning applied (max quality mode).");
    }
    
    Serial.println("[Camera] Init successful!");
    return true;
}

camera_fb_t* CameraHandler::capture() {
    Serial.println("[Camera] Clearing old frame cache...");
    // 连续抓取并丢弃一帧，确保画面为最新且曝光算法（AEC/AWB）收敛
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
        esp_camera_fb_return(fb);
    }
    
    Serial.println("[Camera] Taking actual photo...");
    // 捕获真实硬件 JPEG 数据帧
    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[Camera] Capture failed!");
        return nullptr;
    }
    
    Serial.printf("[Camera] Captured! Size: %.1f KB, Format: %d, Resolution: %ux%u\n", 
                  fb->len / 1024.0f, fb->format, fb->width, fb->height);
    return fb;
}

void CameraHandler::release(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}
