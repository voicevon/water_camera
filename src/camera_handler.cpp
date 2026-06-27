#include "camera_handler.h"
#include <Arduino.h>
#include "config.h"
#include "esp_log.h"
#include <stdarg.h>

// 自定义日志输出拦截函数，用于将 EV-EOF-OVF 缩写展开为带括号的全称与解释
static int custom_log_vprintf(const char *format, va_list args) {
    char buf[256];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(buf, sizeof(buf), format, args_copy);
    va_end(args_copy);

    if (len > 0) {
        if (strstr(buf, "EV-EOF-OVF") != NULL) {
            // 拦截并输出包含全名的详细调试信息
            printf("cam_hal: EV-EOF-OVF (Event End of Frame Overflow: DMA FIFO Buffer Overflow)\n");
            return len;
        }
    }
    return vprintf(format, args);
}

// 实例化全局单例
CameraHandler camera;

bool CameraHandler::init() {
    // 注册自定义日志拦截器以捕获并展开摄像头驱动内部的 EV-EOF-OVF 缩写
    esp_log_set_vprintf(custom_log_vprintf);
    
    // 显式设置摄像头底层日志级别为 VERBOSE
    esp_log_level_set("camera", ESP_LOG_VERBOSE);
    esp_log_level_set("cam_hal", ESP_LOG_VERBOSE);

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
    config.xclk_freq_hz = 10000000;
    
    // 直接硬件输出 JPEG，禁用本地水印，以获取最高画质和帧率
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;
    
    // 使用 config.h 中配置的自定义分辨率和压缩质量 (硬件具备 PSRAM，默认使用双缓冲)
    config.frame_size = CAMERA_FRAME_SIZE;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;
    config.fb_count = 2;                  // 使用双缓冲以提高帧捕获和传输效率
    Serial.printf("[Camera] 配置已应用 - 分辨率宏: %d, JPEG 编码质量(0-63): %d, 双缓冲\n", 
                  config.frame_size, config.jpeg_quality);

    Serial.printf("[Camera] Before esp_camera_init - Free Heap: %u bytes, Free PSRAM: %u bytes\n", 
                  ESP.getFreeHeap(), ESP.getFreePsram());

    // 初始化摄像头
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] Init failed, err: 0x%x\n", err);
        Serial.printf("[Camera] After init failed - Free Heap: %u bytes, Free PSRAM: %u bytes\n", 
                      ESP.getFreeHeap(), ESP.getFreePsram());
        return false;
    }
    
    Serial.printf("[Camera] After init success - Free Heap: %u bytes, Free PSRAM: %u bytes\n", 
                  ESP.getFreeHeap(), ESP.getFreePsram());
    
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
    Serial.printf("[Camera] Start capture - Free Heap: %u bytes, Free PSRAM: %u bytes\n", 
                  ESP.getFreeHeap(), ESP.getFreePsram());

    Serial.println("[Camera] Clearing old frame cache...");
    uint32_t start_time = millis();
    camera_fb_t * fb = esp_camera_fb_get();
    uint32_t duration = millis() - start_time;
    if (fb) {
        Serial.printf("[Camera] Clear old frame cache succeeded. Took %u ms.\n", duration);
        esp_camera_fb_return(fb);
    } else {
        Serial.printf("[Camera] Clear old frame cache failed (nullptr returned). Took %u ms.\n", duration);
    }
    
    Serial.println("[Camera] Taking actual photo...");
    start_time = millis();
    fb = esp_camera_fb_get();
    duration = millis() - start_time;
    if (!fb) {
        Serial.printf("[Camera] Taking actual photo failed (nullptr returned). Took %u ms.\n", duration);
        Serial.printf("[Camera] Failure memory status - Free Heap: %u bytes, Free PSRAM: %u bytes\n", 
                      ESP.getFreeHeap(), ESP.getFreePsram());
        
        // I2C 物理自检，读取 OV2640 内部 PID 和 VER 寄存器
        sensor_t * s = esp_camera_sensor_get();
        if (s == nullptr) {
            Serial.println("[Camera] Fail Diagnosis: Failed to get sensor pointer from esp-camera!");
        } else {
            int pid = s->get_reg(s, 0x0A, 0xFF);
            int ver = s->get_reg(s, 0x0B, 0xFF);
            Serial.printf("[Camera] Fail Diagnosis: Sensor SCCB (I2C) Read test: PID=0x%02X, VER=0x%02X (Expected PID=0x26)\n", 
                          pid, ver);
            if (pid == 0x26) {
                Serial.println("[Camera] Fail Diagnosis result: SCCB (I2C) is communicating normally. Sensor is alive. "
                               "Capture failure is likely due to high-speed data transmission (VSYNC/PCLK signal integrity, noise, or timing).");
            } else {
                Serial.println("[Camera] Fail Diagnosis result: SCCB (I2C) read returned unexpected value or failed. "
                               "Sensor might be dead, powered down (PWDN pin issue), or I2C bus is hung.");
            }
        }
        return nullptr;
    }
    
    Serial.printf("[Camera] Actual capture succeeded. Took %u ms. Size: %.1f KB, Format: %d, Resolution: %ux%u\n", 
                  duration, fb->len / 1024.0f, fb->format, fb->width, fb->height);
    return fb;
}

void CameraHandler::release(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}
