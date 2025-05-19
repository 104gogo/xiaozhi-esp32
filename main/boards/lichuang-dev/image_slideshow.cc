#include "image_slideshow.h"
#include <esp_log.h>

// 包含图片数据
// #include "images/lichuang/doufu/output_0001.h"
// #include "images/lichuang/doufu/output_0002.h"
// #include "images/lichuang/doufu/output_0003.h"
// #include "images/lichuang/doufu/output_0004.h"
// #include "images/lichuang/doufu/output_0005.h"
// #include "images/lichuang/doufu/output_0006.h"
// #include "images/lichuang/doufu/output_0007.h"
// #include "images/lichuang/doufu/output_0008.h"
// #include "images/lichuang/doufu/output_0009.h"
// #include "images/lichuang/doufu/output_0010.h"


// 包含人体检测区域对应的图片
// #include "images/lichuang/doufu_watch2/output_0001.h"
// #include "images/lichuang/doufu_watch2/output_0002.h"
// #include "images/lichuang/doufu_watch2/output_0003.h"
// #include "images/lichuang/doufu_watch2/output_0004.h"
// #include "images/lichuang/doufu_watch2/output_0005.h"
// #include "images/lichuang/doufu_watch2/output_0006.h"
// #include "images/lichuang/doufu_watch2/output_0007.h"
// #include "images/lichuang/doufu_watch2/output_0008.h"
// #include "images/lichuang/doufu_watch2/output_0009.h"
// #include "images/lichuang/doufu_watch2/output_0010.h"
// #include "images/lichuang/doufu_watch2/output_0011.h"
// #include "images/lichuang/doufu_watch2/output_0012.h"
// #include "images/lichuang/doufu_watch2/output_0013.h"


#include "images/lichuang/wangzai/output_0001.h"
#include "images/lichuang/wangzai/output_0002.h"
#include "images/lichuang/wangzai/output_0003.h"
#include "images/lichuang/wangzai/output_0004.h"
#include "images/lichuang/wangzai/output_0005.h"
#include "images/lichuang/wangzai/output_0006.h"
#include "images/lichuang/wangzai/output_0007.h"
#include "images/lichuang/wangzai/output_0008.h"
#include "images/lichuang/wangzai/output_0009.h"
#include "images/lichuang/wangzai/output_0010.h"
#include "images/lichuang/wangzai/output_0011.h"


#include "images/lichuang/wangzai_watch/output_001.h"
#include "images/lichuang/wangzai_watch/output_002.h"
#include "images/lichuang/wangzai_watch/output_003.h"
#include "images/lichuang/wangzai_watch/output_004.h"
#include "images/lichuang/wangzai_watch/output_005.h"
#include "images/lichuang/wangzai_watch/output_006.h"
#include "images/lichuang/wangzai_watch/output_007.h"
#include "images/lichuang/wangzai_watch/output_008.h"
#include "images/lichuang/wangzai_watch/output_009.h"



#define TAG "ImageSlideshow"

// 图片尺寸，所有图片都是相同的尺寸
#define IMG_WIDTH 320
#define IMG_HEIGHT 240

// 定义全局图片数组
static const uint8_t* imageArray[] = {
    gImage_output_0001,
    gImage_output_0002,
    gImage_output_0003,
    gImage_output_0004,
    gImage_output_0005,
    gImage_output_0006,
    gImage_output_0007,
    gImage_output_0008,
    gImage_output_0009,
    gImage_output_0010,
    gImage_output_0011,
    gImage_output_0010,
    gImage_output_0009,
    gImage_output_0008,
    gImage_output_0007,
    gImage_output_0006,
    gImage_output_0005,
    gImage_output_0004,
    gImage_output_0003,
    gImage_output_0002,
    gImage_output_0001
};
static const int totalImages = sizeof(imageArray) / sizeof(imageArray[0]);

// 定义区域检测图片数组
static const uint8_t* regionImageArray[] = {
    watch_output_0001,
    watch_output_0002,
    watch_output_0003,
    watch_output_0004,
    watch_output_0004,
    watch_output_0005,
    watch_output_0005,
    watch_output_0005,
    watch_output_0005,
    watch_output_0006,
    watch_output_0006,
    watch_output_0007,
    watch_output_0008,
    watch_output_0009,
};
static const int totalRegionImages = sizeof(regionImageArray) / sizeof(regionImageArray[0]);

// 构造函数
ImageSlideshow::ImageSlideshow(Display* display)
    : display_(display), task_handle_(nullptr), region_check_task_handle_(nullptr), 
      is_running_(false), last_region_index_(-1) {
}

// 析构函数
ImageSlideshow::~ImageSlideshow() {
    Stop();
    StopRegionCheck();
}

// 启动图片循环显示任务
void ImageSlideshow::Start() {
    if (!is_running_ && display_) {
        // 直接重置本地区域索引变量
        last_region_index_ = -1;
        
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 4096, this, 3, &task_handle_);
        is_running_ = true;
        ESP_LOGI(TAG, "图片循环显示任务已启动");
    }
}

// 停止图片循环显示任务
void ImageSlideshow::Stop() {
    if (is_running_ && task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
        is_running_ = false;
        ESP_LOGI(TAG, "图片循环显示任务已停止");
    }
}

// 启动区域检测任务
void ImageSlideshow::StartRegionCheck() {
    if (region_check_task_handle_ == nullptr) {
        // 增加栈大小至4096字节，防止栈溢出
        xTaskCreate(RegionCheckTask, "region_check", 4096, this, 2, &region_check_task_handle_);
        ESP_LOGI(TAG, "区域索引检测任务已启动");
    }
}

// 停止区域检测任务
void ImageSlideshow::StopRegionCheck() {
    if (region_check_task_handle_ != nullptr) {
        vTaskDelete(region_check_task_handle_);
        region_check_task_handle_ = nullptr;
        ESP_LOGI(TAG, "区域检测任务已停止");
    }
}

// 根据人体检测区域索引绘制特定图片
void ImageSlideshow::DrawImageByRegionIndex(int region_index) {    
    // 计算新的索引值（减3）并确保不会越界
    int adjusted_index = region_index - 3;
    if (adjusted_index < 0) {
        adjusted_index = 0; // 如果越界则使用第一张图片
    }

    if (adjusted_index >= totalRegionImages) {
        adjusted_index = totalRegionImages - 1;
    }
    
    
    // 绘制对应的区域图片
    ESP_LOGI(TAG, "绘制区域 %d 对应的图片（原始索引：%d）", adjusted_index, region_index);
    
    // 确保画布已创建
    if (!display_->HasCanvas()) {
        display_->CreateCanvas();
    }
    
    // 从区域图片数组中获取对应图片并绘制
    const uint8_t* image = regionImageArray[adjusted_index];
    
    // 转换并绘制图片
    uint16_t* convertedData = new uint16_t[IMG_WIDTH * IMG_HEIGHT];
    if (!convertedData) {
        ESP_LOGE(TAG, "无法分配内存进行图像转换");
        return;
    }
    
    // 转换图片数据的字节序
    for (int i = 0; i < IMG_WIDTH * IMG_HEIGHT; i++) {
        uint16_t pixel = ((uint16_t*)image)[i];
        convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
    }
    
    // 在画布上绘制图片
    display_->DrawImageOnCanvas(0, 0, IMG_WIDTH, IMG_HEIGHT, (const uint8_t*)convertedData);
    
    // 释放内存
    delete[] convertedData;
}

// 根据索引获取图片并绘制到画布上
void ImageSlideshow::DrawImageByIndex(int imgIndex) {
    // 确保索引在有效范围内
    if (imgIndex < 0 || imgIndex >= totalImages) {
        imgIndex = 0;
    }
    
    // 获取图片指针
    const uint8_t* image = imageArray[imgIndex];
    
    // 转换并绘制图片
    uint16_t* convertedData = new uint16_t[IMG_WIDTH * IMG_HEIGHT];
    if (!convertedData) {
        ESP_LOGE(TAG, "无法分配内存进行图像转换");
        return;
    }
    
    // 转换图片数据的字节序
    for (int i = 0; i < IMG_WIDTH * IMG_HEIGHT; i++) {
        uint16_t pixel = ((uint16_t*)image)[i];
        convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
    }
    
    // 在画布上绘制图片
    display_->DrawImageOnCanvas(0, 0, IMG_WIDTH, IMG_HEIGHT, (const uint8_t*)convertedData);
    
    // 释放内存
    delete[] convertedData;
}

// 图片循环显示任务函数
void ImageSlideshow::ImageSlideshowTask(void* arg) {
    ImageSlideshow* slideshow = static_cast<ImageSlideshow*>(arg);
    Display* display = slideshow->display_;
    
    if (!display) {
        ESP_LOGE(TAG, "无法获取显示设备");
        vTaskDelete(NULL);
        return;
    }
    
    // 获取Application实例
    auto& app = Application::GetInstance();
    
    // 创建画布（如果不存在）
    if (!display->HasCanvas()) {
        display->CreateCanvas();
    }
    
    // 先显示第一张图片
    int currentIndex = 0;
    slideshow->DrawImageByIndex(currentIndex);
    ESP_LOGI(TAG, "初始显示图片");
    
    // 持续监控和处理图片显示
    TickType_t lastUpdateTime = xTaskGetTickCount();
    const TickType_t cycleInterval = pdMS_TO_TICKS(60); // 图片切换间隔60毫秒
    
    // 定义用于判断是否正在播放音频的变量
    bool isAudioPlaying = false;
    bool wasAudioPlaying = false;
    
    while (true) {
        // 检查是否正在播放音频 - 使用应用程序状态判断
        isAudioPlaying = (app.GetDeviceState() == kDeviceStateSpeaking);
        
        TickType_t currentTime = xTaskGetTickCount();
        
        // 如果正在播放音频且时间到了切换间隔
        if (isAudioPlaying && (currentTime - lastUpdateTime >= cycleInterval)) {
            // 更新索引到下一张图片
            currentIndex = (currentIndex + 1) % totalImages;
            
            // 绘制新图片
            slideshow->DrawImageByIndex(currentIndex);
            
            // 更新上次更新时间
            lastUpdateTime = currentTime;
        }
        // 如果不在播放音频但上一次检查时在播放，或者当前不在第一张图片
        else if ((!isAudioPlaying && wasAudioPlaying) || (!isAudioPlaying && currentIndex != 0)) {
            // 切换回第一张图片
            currentIndex = 0;
            
            // 绘制初始图片
            slideshow->DrawImageByIndex(currentIndex);
            ESP_LOGI(TAG, "返回显示初始图片");
        }
        
        // 更新上一次音频播放状态
        wasAudioPlaying = isAudioPlaying;
        
        // 短暂延时，避免CPU占用过高
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

// 区域检测任务函数
void ImageSlideshow::RegionCheckTask(void* arg) {
    ImageSlideshow* slideshow = static_cast<ImageSlideshow*>(arg);
    if (!slideshow) {
        ESP_LOGE(TAG, "无法获取图片显示实例");
        vTaskDelete(NULL);
        return;
    }
    
    auto& app = Application::GetInstance();
    
    // 等待一段时间，确保所有系统组件都已初始化
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (true) {
        // 获取当前设备状态
        DeviceState state = app.GetDeviceState();
        
        // 只在聆听状态下处理区域索引
        if (state == kDeviceStateListening) {
            int current_region_index = app.GetProtocol().region_index();
            
            // 检查区域索引是否发生变化
            if (current_region_index != slideshow->last_region_index_) {
                ESP_LOGI(TAG, "区域索引变化: %d -> %d", 
                         slideshow->last_region_index_, current_region_index);
                
                // 绘制新图片
                slideshow->DrawImageByRegionIndex(current_region_index);
                
                // 更新记录的索引
                slideshow->last_region_index_ = current_region_index;
            }
        }
        
        // 延时
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// 重置区域索引，使其回到初始状态
void ImageSlideshow::ResetRegionIndex() {
    last_region_index_ = -1;
    ESP_LOGI(TAG, "区域索引已重置");
    
    // 注意：不再尝试更新Protocol中的region_index，避免可能的空指针或初始化问题
} 