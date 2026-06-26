#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "esp_camera.h"

class CameraHandler {
public:
    CameraHandler() = default;

    /**
     * @brief 初始化摄像头硬件并应用优化画质参数
     * @return bool 是否初始化成功
     */
    bool init();

    /**
     * @brief 抓取一帧新鲜照片（丢弃旧帧缓存以确保最新曝光）
     * @return camera_fb_t* 帧缓冲区指针，失败返回 nullptr
     */
    camera_fb_t* capture();

    /**
     * @brief 释放帧缓冲区内存
     * @param fb 帧缓冲区指针
     */
    void release(camera_fb_t* fb);
};

// 全局单例声明
extern CameraHandler camera;

#endif // CAMERA_HANDLER_H
