#include "opus_processor.h"
#include <cstring>
#include <arpa/inet.h>

const char* OpusProcessor::TAG = "OpusProcessor";

OpusProcessor::OpusProcessor(Display* display) : display_(display) {
}

bool OpusProcessor::ProcessAndSendOpusData(const std::vector<uint8_t>& opus_data, bool need_connect) {
    // 创建一个string_view
    std::string_view opus_view(reinterpret_cast<const char*>(opus_data.data()), opus_data.size());
    return ProcessAndSendOpusData(opus_view, need_connect);
}

bool OpusProcessor::ProcessAndSendOpusData(const std::string_view& opus_view, bool need_connect) {
    ESP_LOGI(TAG, "开始处理Opus数据");
    
    // 显示数据大小和前几个字节，用于调试
    const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(opus_view.data());
    size_t data_size = opus_view.size();
    
    ESP_LOGI(TAG, "Opus数据总大小: %d字节", data_size);
    if (data_size >= 16) {
        ESP_LOGI(TAG, "前16个字节: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                data_ptr[0], data_ptr[1], data_ptr[2], data_ptr[3], 
                data_ptr[4], data_ptr[5], data_ptr[6], data_ptr[7],
                data_ptr[8], data_ptr[9], data_ptr[10], data_ptr[11], 
                data_ptr[12], data_ptr[13], data_ptr[14], data_ptr[15]);
    }
    
    auto& app = Application::GetInstance();

    // 通过Protocol将opus数据发送到服务器
    ESP_LOGI(TAG, "准备将音频数据发送到服务器...");
    if (display_) {
        // display_->ShowNotification("正在连接服务器...");
    }
    
    // 获取Protocol实例
    Protocol* protocol = &app.GetProtocol();
    
    // 连接到Protocol服务器并打开音频通道
    if (need_connect && !protocol->IsAudioChannelOpened()) {
        ESP_LOGI(TAG, "正在连接服务器...");
        if (display_) {
            // display_->ShowNotification("正在连接服务器...");
        }
        
        // 尝试打开音频通道（连接服务器）
        if (!protocol->OpenAudioChannel()) {
            ESP_LOGE(TAG, "无法连接到服务器，发送失败");
            if (display_) {
                // display_->ShowNotification("无法连接到服务器，发送失败");
            }
            return false;
        }
        
        // 给服务器连接一些时间稳定
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "服务器连接成功");
        if (display_) {
            // display_->ShowNotification("服务器连接成功");
        }
    } else if (!protocol->IsAudioChannelOpened()) {
        ESP_LOGE(TAG, "服务器未连接");
        if (display_) {
            // display_->ShowNotification("服务器未连接");
        }
        return false;
    } else {
        ESP_LOGI(TAG, "已连接到服务器");
    }
    
    // 确保连接仍然有效
    if (!protocol->IsAudioChannelOpened()) {
        ESP_LOGE(TAG, "服务器连接已断开，发送失败");
        if (display_) {
            // display_->ShowNotification("服务器连接已断开，发送失败");
        }
        return false;
    }
    
    if (display_) {
        // display_->ShowNotification("正在发送音频数据...");
    }
    
    // 解析P3格式数据中的opus包
    const char* data = opus_view.data();
    size_t size = opus_view.size();
    int packet_count = 0;
    
    // 定义P3头部结构
    struct BinaryProtocol3 {
        uint8_t packet_type;
        uint8_t reserved;
        uint16_t payload_size;
        uint8_t payload[0];
    };
    
    for (const char* p = data; p < data + size; ) {
        auto p3 = (BinaryProtocol3*)p;
        p += sizeof(BinaryProtocol3);
        
        auto payload_size = ntohs(p3->payload_size);
        std::vector<uint8_t> opus_packet;
        opus_packet.resize(payload_size);
        memcpy(opus_packet.data(), p3->payload, payload_size);
        p += payload_size;
        
        // 发送opus包到服务器
        AudioStreamPacket audio_packet;
        audio_packet.payload = std::move(opus_packet);
        audio_packet.timestamp = 0; // 如果需要设置时间戳，这里可以修改
        protocol->SendAudio(audio_packet);
        packet_count++;
        ESP_LOGI(TAG, "已发送opus包 %d，大小: %d字节", packet_count, payload_size);
    }
    
    ESP_LOGI(TAG, "所有opus数据已发送到服务器，共 %d 个包", packet_count);
    if (display_) {
        // display_->ShowNotification("音频数据已发送完成，共 " + std::to_string(packet_count) + " 个包");
    }
    
    return true;
} 