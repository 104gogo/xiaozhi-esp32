#include "protocol.h"

#include <esp_log.h>
#include <mbedtls/base64.h>

#define TAG "Protocol"

void Protocol::OnIncomingJson(std::function<void(const cJSON* root)> callback) {
    on_incoming_json_ = callback;
}

void Protocol::OnIncomingAudio(std::function<void(AudioStreamPacket&& packet)> callback) {
    on_incoming_audio_ = callback;
}

void Protocol::OnAudioChannelOpened(std::function<void()> callback) {
    on_audio_channel_opened_ = callback;
}

void Protocol::OnAudioChannelClosed(std::function<void()> callback) {
    on_audio_channel_closed_ = callback;
}

void Protocol::OnNetworkError(std::function<void(const std::string& message)> callback) {
    on_network_error_ = callback;
}

void Protocol::SetError(const std::string& message) {
    error_occurred_ = true;
    if (on_network_error_ != nullptr) {
        on_network_error_(message);
    }
}

void Protocol::SendAbortSpeaking(AbortReason reason) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"abort\"";
    if (reason == kAbortReasonWakeWordDetected) {
        message += ",\"reason\":\"wake_word_detected\"";
    }
    message += "}";
    SendText(message);
}

void Protocol::SendWakeWordDetected(const std::string& wake_word) {
    std::string json = "{\"session_id\":\"" + session_id_ + 
                      "\",\"type\":\"listen\",\"state\":\"detect\",\"text\":\"" + wake_word + "\"}";
    SendText(json);
}

void Protocol::SendStartListening(ListeningMode mode) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\"";
    message += ",\"type\":\"listen\",\"state\":\"start\"";
    if (mode == kListeningModeRealtime) {
        message += ",\"mode\":\"realtime\"";
    } else if (mode == kListeningModeAutoStop) {
        message += ",\"mode\":\"auto\"";
    } else {
        message += ",\"mode\":\"manual\"";
    }
    message += "}";
    SendText(message);
}

void Protocol::SendStopListening() {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"listen\",\"state\":\"stop\"}";
    SendText(message);
}

void Protocol::SendIotDescriptors(const std::string& descriptors) {
    cJSON* root = cJSON_Parse(descriptors.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse IoT descriptors: %s", descriptors.c_str());
        return;
    }

    if (!cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "IoT descriptors should be an array");
        cJSON_Delete(root);
        return;
    }

    int arraySize = cJSON_GetArraySize(root);
    for (int i = 0; i < arraySize; ++i) {
        cJSON* descriptor = cJSON_GetArrayItem(root, i);
        if (descriptor == nullptr) {
            ESP_LOGE(TAG, "Failed to get IoT descriptor at index %d", i);
            continue;
        }

        cJSON* messageRoot = cJSON_CreateObject();
        cJSON_AddStringToObject(messageRoot, "session_id", session_id_.c_str());
        cJSON_AddStringToObject(messageRoot, "type", "iot");
        cJSON_AddBoolToObject(messageRoot, "update", true);

        cJSON* descriptorArray = cJSON_CreateArray();
        cJSON_AddItemToArray(descriptorArray, cJSON_Duplicate(descriptor, 1));
        cJSON_AddItemToObject(messageRoot, "descriptors", descriptorArray);

        char* message = cJSON_PrintUnformatted(messageRoot);
        if (message == nullptr) {
            ESP_LOGE(TAG, "Failed to print JSON message for IoT descriptor at index %d", i);
            cJSON_Delete(messageRoot);
            continue;
        }

        SendText(std::string(message));
        cJSON_free(message);
        cJSON_Delete(messageRoot);
    }

    cJSON_Delete(root);
}

void Protocol::SendIotStates(const std::string& states) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"iot\",\"update\":true,\"states\":" + states + "}";
    SendText(message);
}

bool Protocol::IsTimeout() const {
    const int kTimeoutSeconds = 600;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_incoming_time_);
    bool timeout = duration.count() > kTimeoutSeconds;
    if (timeout) {
        ESP_LOGE(TAG, "Channel timeout %lld seconds", duration.count());
    }
    return timeout;
}

bool Protocol::IsAudioChannelBusy() const {
    return busy_sending_audio_;
}

void Protocol::SendIotCameraPhoto(const std::vector<uint8_t>& photo_data, int width, int height, const std::string& format) {
    if (photo_data.empty()) {
        ESP_LOGE(TAG, "照片数据为空，无法发送");
        return;
    }

    // 计算Base64编码后的大小
    size_t base64_len;
    mbedtls_base64_encode(nullptr, 0, &base64_len, photo_data.data(), photo_data.size());
    
    // 分配内存
    unsigned char* base64_buf = new unsigned char[base64_len];
    if (base64_buf == nullptr) {
        ESP_LOGE(TAG, "无法分配内存进行Base64编码");
        return;
    }
    
    // 执行Base64编码
    int ret = mbedtls_base64_encode(base64_buf, base64_len, &base64_len, photo_data.data(), photo_data.size());
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64编码失败: %d", ret);
        delete[] base64_buf;
        return;
    }
    
    // 构建JSON消息
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"iot\",\"update\":true,\"camera_photo\":{";
    message += "\"width\":" + std::to_string(width) + ",";
    message += "\"height\":" + std::to_string(height) + ",";
    message += "\"format\":\"" + format + "\",";
    message += "\"data\":\"" + std::string(reinterpret_cast<char*>(base64_buf), base64_len) + "\"";
    message += "}}";
    
    // 释放内存
    delete[] base64_buf;
    
    // 发送消息
    ESP_LOGI(TAG, "正在发送照片数据到服务器，大小: %zu 字节", photo_data.size());
    SendText(message);
}

void Protocol::OnCameraPhotoResponse(const cJSON* response_json) {
    // 获取状态和消息
    auto status = cJSON_GetObjectItem(response_json, "status");
    auto message = cJSON_GetObjectItem(response_json, "message");
    auto filename = cJSON_GetObjectItem(response_json, "filename");
    auto region_index = cJSON_GetObjectItem(response_json, "region_index");
    
    // 打印日志
    if (status && message) {
        ESP_LOGI(TAG, "相机照片处理响应: 状态=%s, 消息=%s", 
                 status->valuestring, message->valuestring);
    }
    
    if (filename) {
        ESP_LOGI(TAG, "照片已保存为: %s", filename->valuestring);
    }
    
    // 处理区域索引并保存到成员变量
    if (region_index) {
        if (cJSON_IsNumber(region_index)) {
            int index = region_index->valueint;
            ESP_LOGI(TAG, "检测到人体，位于区域: %d", index);
            // 保存区域索引到成员变量
            set_region_index(index);
        } else if (cJSON_IsString(region_index)) {
            ESP_LOGI(TAG, "检测到人体，位于区域: %s", region_index->valuestring);
            // 尝试将字符串转换为整数
            try {
                int index = std::stoi(region_index->valuestring);
                set_region_index(index);
            } catch (const std::exception& e) {
                ESP_LOGW(TAG, "无法将区域索引字符串 '%s' 转换为整数: %s", 
                         region_index->valuestring, e.what());
            }
        }
    }
}
