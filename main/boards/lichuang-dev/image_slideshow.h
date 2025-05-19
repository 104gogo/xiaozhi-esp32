#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "display/lcd_display.h"
#include "application.h"

// 前向声明
class Display;

class ImageSlideshow {
public:
    // 构造函数
    ImageSlideshow(Display* display);
    
    // 析构函数
    ~ImageSlideshow();
    
    // 启动图片循环显示任务
    void Start();
    
    // 停止图片循环显示任务
    void Stop();
    
    // 根据人体检测区域索引绘制特定图片
    void DrawImageByRegionIndex(int region_index);
    
    // 启动区域检测任务 - 监视Protocol中的区域索引变化并显示对应图片
    void StartRegionCheck();
    
    // 停止区域检测任务
    void StopRegionCheck();
    
    // 重置区域索引，使其回到初始状态
    void ResetRegionIndex();
    
private:
    // 图片循环显示任务函数
    static void ImageSlideshowTask(void* arg);
    
    // 区域检测任务函数 - 检查Protocol中的区域索引变化
    static void RegionCheckTask(void* arg);
    
    // 根据索引获取图片并绘制到画布上
    void DrawImageByIndex(int imgIndex);
    
    // 成员变量
    Display* display_;
    TaskHandle_t task_handle_;
    TaskHandle_t region_check_task_handle_;
    bool is_running_;
    int last_region_index_;
}; 