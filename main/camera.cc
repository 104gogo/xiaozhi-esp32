#include "camera.h"
#include "protocols/protocol.h"
#include "application.h"
#include "board.h"

#include <esp_log.h>
#include <esp_camera.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define TAG "StreamCamera"

static TaskHandle_t xStreamTaskHandle = NULL;
static bool streaming_enabled = false;
static SemaphoreHandle_t stream_mutex = NULL;
static bool camera_initialized = false;

void Camera::init() {
    // 创建互斥锁
    if (stream_mutex == NULL) {
        stream_mutex = xSemaphoreCreateMutex();
        if (stream_mutex == NULL) {
            ESP_LOGE(TAG, "创建互斥锁失败");
            return;
        }
    }
    
    // 检查摄像头状态
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL) {
        ESP_LOGE(TAG, "摄像头传感器不可用，初始化失败");
        camera_initialized = false;
    } else {
        camera_initialized = true;
        ESP_LOGI(TAG, "摄像头初始化成功，传感器ID: 0x%x", sensor->id.PID);
    }
    
    ESP_LOGI(TAG, "摄像头初始化完成");
}

void Camera::start_streaming() {
    if (!camera_initialized) {
        ESP_LOGE(TAG, "摄像头未初始化成功，无法启动流式传输");
        return;
    }
    
    if (xStreamTaskHandle != NULL) {
        ESP_LOGW(TAG, "流式传输任务已经在运行");
        return;
    }
    
    // 检查摄像头状态
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL) {
        ESP_LOGE(TAG, "无法获取摄像头传感器");
        return;
    }
    
    // 设置摄像头参数
    sensor->set_framesize(sensor, FRAMESIZE_QVGA);  // 320x240
    sensor->set_pixformat(sensor, PIXFORMAT_RGB565);
    
    // 启用流式传输标志
    streaming_enabled = true;
    
    // 创建流式传输任务
    BaseType_t task_created = xTaskCreatePinnedToCore(
        stream_task,
        "stream_task",
        4 * 1024,  // 为任务分配足够的堆栈
        NULL,
        2,  // 低优先级
        &xStreamTaskHandle,
        1);  // 在核心1运行
        
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "创建流式传输任务失败: %d", task_created);
        streaming_enabled = false;
    } else {
        ESP_LOGI(TAG, "流式传输任务创建成功");
    }
}

void Camera::stop_streaming() {
    // 禁用流式传输标志
    streaming_enabled = false;
    
    // 等待任务自然结束
    vTaskDelay(pdMS_TO_TICKS(300));
    
    // 重置任务句柄
    xStreamTaskHandle = NULL;
    
    ESP_LOGI(TAG, "流式传输已停止");
}

bool Camera::is_streaming() {
    return streaming_enabled;
}

bool Camera::is_initialized() {
    return camera_initialized;
}

// 流式传输任务
void Camera::stream_task(void *arg) {
    ESP_LOGI(TAG, "开始流式传输任务");
    int frame_count = 0;
    int error_count = 0;
    int consecutive_errors = 0;
    
    while (streaming_enabled) {
        // 获取摄像头帧
        camera_fb_t *frame = esp_camera_fb_get();
        
        if (frame && frame->buf && frame->len > 0) {
            frame_count++;
            consecutive_errors = 0; // 重置连续错误计数
            
            if (frame_count % 10 == 0) {
                ESP_LOGI(TAG, "已捕获和发送 %d 帧图像, 大小: %dx%d, 格式: %d", 
                         frame_count, frame->width, frame->height, frame->format);
            }
            
            // 处理并发送帧
            process_and_send_frame(frame);
            
            // 返回帧缓冲区
            esp_camera_fb_return(frame);
        } else {
            error_count++;
            consecutive_errors++;
            
            if (error_count % 10 == 0) {
                ESP_LOGE(TAG, "获取摄像头帧失败 (错误计数: %d)", error_count);
            }
            
            if (frame) esp_camera_fb_return(frame);
            
            // 如果连续错误超过阈值，退出任务
            if (consecutive_errors > 10) {
                ESP_LOGE(TAG, "摄像头连续错误过多，停止流式传输");
                streaming_enabled = false;
                break;
            }
        }
        
        // 等待约100ms再获取下一帧
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "流式传输任务退出，共处理 %d 帧", frame_count);
    vTaskDelete(NULL);
}

void Camera::process_and_send_frame(camera_fb_t* frame) {
    if (!frame) {
        ESP_LOGE(TAG, "帧为空，无法处理");
        return;
    }
    
    // 获取Protocol实例
    Protocol* protocol = &Application::GetInstance().GetProtocol();
    if (!protocol) {
        ESP_LOGE(TAG, "无法获取协议实例");
        return;
    }
    
    // 创建数据向量
    std::vector<uint8_t> photo_data;
    std::string format = "jpg"; // 默认格式
    
    // 检查图像大小，如果过大需要压缩
    const size_t MAX_WEBSOCKET_SIZE = 60000; // 略小于65535
    
    if (frame->format == PIXFORMAT_RGB565) {
        // 将RGB565转换为JPEG以减小大小
        size_t out_len = 0;
        uint8_t* out_buf = NULL;
        const int quality = 60; // 降低JPEG质量以减小文件大小
        bool jpeg_converted = frame2jpg(frame, quality, &out_buf, &out_len);
        
        if (jpeg_converted && out_len <= MAX_WEBSOCKET_SIZE) {
            ESP_LOGI(TAG, "成功将RGB565转换为JPEG，大小: %d 字节", out_len);
            photo_data.assign(out_buf, out_buf + out_len);
            free(out_buf); // 释放esp-camera分配的内存
        } else {
            if (out_buf) free(out_buf);
            
            // 尝试更低的质量
            out_len = 0;
            out_buf = NULL;
            const int lower_quality = 60; // 降低质量但仍保持良好水平
            jpeg_converted = frame2jpg(frame, lower_quality, &out_buf, &out_len);
            
            if (jpeg_converted && out_len <= MAX_WEBSOCKET_SIZE) {
                ESP_LOGI(TAG, "使用更低质量(%d)成功将RGB565转换为JPEG，大小: %d 字节", 
                         lower_quality, out_len);
                photo_data.assign(out_buf, out_buf + out_len);
                free(out_buf);
            } else {
                if (out_buf) free(out_buf);
                ESP_LOGE(TAG, "无法将图像压缩到合适的大小");
                return;
            }
        }
    } else {
        ESP_LOGE(TAG, "不支持的图像格式: %d", frame->format);
        format = "unknown";
        return;
    }
    
    if (photo_data.empty()) {
        ESP_LOGE(TAG, "照片数据为空，无法发送");
        return;
    }
    
    // 发送照片数据
    protocol->SendIotCameraPhoto(photo_data, frame->width, frame->height, format);
}

Camera::Camera() {
    init();
}

Camera::~Camera() {
    stop_streaming();
    
    if (stream_mutex != NULL) {
        vSemaphoreDelete(stream_mutex);
        stream_mutex = NULL;
    }
} 