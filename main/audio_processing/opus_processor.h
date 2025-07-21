#ifndef OPUS_PROCESSOR_H
#define OPUS_PROCESSOR_H

#include <vector>
#include <string>
#include <string_view>
#include <esp_log.h>
#include "application.h"
#include "display.h"
#include "protocols/protocol.h"

/**
 * @brief 用于处理Opus音频数据的类
 * 
 * 这个类提供了读取、处理和发送Opus格式音频数据的功能
 */
class OpusProcessor {
public:
    /**
     * @brief 构造函数
     * 
     * @param display 显示器指针，用于显示处理状态
     */
    OpusProcessor(Display* display);

    /**
     * @brief 处理并发送Opus数据
     * 
     * 从给定的数据中提取Opus包并通过Protocol发送
     * 
     * @param opus_data 包含Opus数据的向量
     * @param need_connect 是否需要先连接服务器
     * @return true 处理成功
     * @return false 处理失败
     */
    bool ProcessAndSendOpusData(const std::vector<uint8_t>& opus_data, bool need_connect = true);
    
    /**
     * @brief 处理并发送Opus数据
     * 
     * 从给定的数据中提取Opus包并通过Protocol发送
     * 
     * @param opus_view 包含Opus数据的字符串视图
     * @param need_connect 是否需要先连接服务器
     * @return true 处理成功
     * @return false 处理失败
     */
    bool ProcessAndSendOpusData(const std::string_view& opus_view, bool need_connect = true);

private:
    Display* display_;  // 显示器指针
    static const char* TAG;  // 日志标签
};

#endif // OPUS_PROCESSOR_H 