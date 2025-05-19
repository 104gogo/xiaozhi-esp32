#pragma once

#include <esp_camera.h>
#include <vector>
#include <string>

/**
 * @brief 摄像头类，提供定期捕获和发送照片的功能
 * 
 * 该类可以每150ms从摄像头捕获一张照片，并通过Protocol::SendIotCameraPhoto发送出去
 */
class Camera {
public:
    /**
     * @brief 构造函数
     */
    Camera();
    
    /**
     * @brief 析构函数
     */
    ~Camera();
    
    /**
     * @brief 初始化摄像头和相关资源
     */
    void init();
    
    /**
     * @brief 开始流式传输
     * 
     * 启动一个任务，每150ms捕获一张照片并发送
     */
    void start_streaming();
    
    /**
     * @brief 停止流式传输
     */
    void stop_streaming();
    
    /**
     * @brief 检查是否正在流式传输
     * 
     * @return true 如果正在流式传输
     * @return false 如果没有流式传输
     */
    bool is_streaming();
    
    /**
     * @brief 检查摄像头是否成功初始化
     * 
     * @return true 如果摄像头初始化成功
     * @return false 如果摄像头初始化失败
     */
    bool is_initialized();

private:
    /**
     * @brief 流式传输任务
     * 
     * @param arg 任务参数（未使用）
     */
    static void stream_task(void *arg);
    
    /**
     * @brief 处理并发送一帧图像
     * 
     * @param frame 摄像头帧
     */
    static void process_and_send_frame(camera_fb_t* frame);
}; 