#include "esp32_livestream.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "audio_codec.h"
#include "websocket_protocol.h"
#include "settings.h"
#include "application.h"
#include "device_state_event.h"
#include <esp_log.h>
#include <esp_app_desc.h>
#include <cJSON.h>
#include <chrono>

#define TAG "Esp32Livestream"

Esp32Livestream::Esp32Livestream() {
    ESP_LOGI(TAG, "Livestream client initialized");
}

Esp32Livestream::~Esp32Livestream() {
    ESP_LOGI(TAG, "Livestream client deinitialized");
    StopReconnectTimer();
    DisconnectFromServer();
}

std::string Esp32Livestream::GetLivestreamServerUrl() const {
    return "ws://" + std::string(SERVER_IP) + ":" + std::to_string(LIVESTREAM_PORT) + "/xiaozhi/v1/";
}

bool Esp32Livestream::IsConnected() {
    return connected_ && protocol_ && protocol_->IsAudioChannelOpened();
}

std::string Esp32Livestream::GetStatus() {
    std::string status = "{";
    status += "\"connected\":" + std::string(IsConnected() ? "true" : "false") + ",";
    status += "\"server_url\":\"" + GetLivestreamServerUrl() + "\",";
    if (!last_error_.empty()) {
        status += ",\"last_error\":\"" + last_error_ + "\"";
    }
    status += "}";
    return status;
}

bool Esp32Livestream::ConnectToServer() {
    // 断开现有连接
    DisconnectFromServer();
    
    last_error_.clear();
    auto app_desc = esp_app_get_description();
    
    // 配置WebSocket设置 - 必须在创建协议之前设置，使用写入权限
    {
        Settings websocket_settings("websocket", true); // 启用写入权限
        websocket_settings.SetString("url", GetLivestreamServerUrl());
        websocket_settings.SetInt("version", 1); // 使用版本1保持兼容性
        websocket_settings.SetString("appVersion", app_desc->version); 
        websocket_settings.SetString("boardName", BOARD_NAME); 

        // 获取设备状态信息并存储到settings中
        try {
            auto& board = Board::GetInstance();
            std::string device_status = board.GetDeviceStatusJson();
            if (!device_status.empty() && device_status.length() > 2) { // 至少包含 "{}"
                websocket_settings.SetString("deviceStatus", device_status);
                ESP_LOGI(TAG, "Device status saved to websocket settings (%d bytes)", (int)device_status.length());
            } else {
                ESP_LOGW(TAG, "Device status is empty or invalid, skipping header setup");
            }
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Failed to get device status: %s", e.what());
        }

    } // Settings对象析构时自动保存
    
    // 验证设置是否正确保存
    {
        Settings verify_settings("websocket", false);
        std::string saved_url = verify_settings.GetString("url");
        // ESP_LOGI(TAG, "Verified saved URL: %s", saved_url.c_str());
        if (saved_url != GetLivestreamServerUrl()) {
            last_error_ = "Failed to save WebSocket URL settings";
            return false;
        }
    }
    
    // 创建WebsocketProtocol实例
    protocol_ = std::make_unique<WebsocketProtocol>();
    
    // 设置消息处理回调
    protocol_->OnIncomingJson([this](const cJSON* root) {
        HandleIncomingJson(root);
    });
    
    protocol_->OnAudioChannelOpened([this]() {
        ESP_LOGI(TAG, "WebSocket audio channel opened for livestream");
        connected_ = true;
        
        // 连接成功，停止重连定时器并重置重连次数
        StopReconnectTimer();
        reconnect_attempts_ = 0;
    });
    
    protocol_->OnAudioChannelClosed([this]() {
        ESP_LOGI(TAG, "WebSocket audio channel closed for livestream");
        connected_ = false;
        
        // 启动重连定时器
        ESP_LOGI(TAG, "Audio channel closed, starting reconnect timer");
        StartReconnectTimer();
    });
    
    protocol_->OnNetworkError([this](const std::string& message) {
        ESP_LOGE(TAG, "Network error: %s", message.c_str());
        last_error_ = message;
        connected_ = false;
        
        // 启动重连定时器
        ESP_LOGI(TAG, "Network error occurred, starting reconnect timer");
        StartReconnectTimer();
    });
    
    // 启动协议（这将初始化内部状态）
    if (!protocol_->Start()) {
        last_error_ = "Failed to start WebSocket protocol";
        ESP_LOGE(TAG, "%s", last_error_.c_str());
        protocol_.reset();
        return false;
    }
    
    // 尝试建立连接
    if (!protocol_->OpenAudioChannel()) {
        last_error_ = "Failed to open WebSocket audio channel";
        ESP_LOGE(TAG, "%s", last_error_.c_str());
        protocol_.reset();
        return false;
    }
    
    ESP_LOGI(TAG, "Successfully initiated connection to livestream server");
    return true;
}

void Esp32Livestream::DisconnectFromServer() {
    ESP_LOGI(TAG, "Starting disconnect from livestream server");
    
    // 停止重连定时器
    StopReconnectTimer();
    
    if (protocol_) {
        ESP_LOGI(TAG, "Closing audio channel and resetting protocol");
        protocol_->CloseAudioChannel();
        protocol_.reset();
    }
    connected_ = false;
    
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(""); // 清空状态显示
    
    ESP_LOGI(TAG, "Livestream server disconnection completed");
}

void Esp32Livestream::SendLivestreamHello() {
    if (!protocol_) {
        return;
    }
    
    ESP_LOGI(TAG, "WebSocket connection established, livestream mode ready");
}

void Esp32Livestream::HandleIncomingJson(const cJSON* root) {
    // 打印接收到的完整JSON消息用于调试
    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        // ESP_LOGI(TAG, "Received complete JSON message: %s", json_str);
        cJSON_free(json_str);
    }
    
    auto type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        ESP_LOGW(TAG, "Received message without type field");
        return;
    }
    
    std::string type_str = type->valuestring;
    // ESP_LOGI(TAG, "Received message type: %s", type_str.c_str());
    
    // 获取公共字段
    cJSON* content = cJSON_GetObjectItem(root, "content");
    cJSON* sender_device_id = cJSON_GetObjectItem(root, "sender_device_id");
    std::string sender_id = cJSON_IsString(sender_device_id) ? sender_device_id->valuestring : "";
    
    if (type_str == "welcome") {
        ESP_LOGI(TAG, "Received welcome message from livestream server");
    } else if (type_str == "admin_message") {
        cJSON* device_id_item = cJSON_GetObjectItem(root, "deviceId");
        std::string device_id_str = cJSON_IsString(device_id_item) ? device_id_item->valuestring : "";
        cJSON* messageType_item = cJSON_GetObjectItem(root, "messageType");
        std::string messageType_str = cJSON_IsString(messageType_item) ? messageType_item->valuestring : "text";
        cJSON* sender_item = cJSON_GetObjectItem(root, "sender");
        std::string sender_str = cJSON_IsString(sender_item) ? sender_item->valuestring : "";
        cJSON* timestamp_item = cJSON_GetObjectItem(root, "timestamp");
        int64_t timestamp = cJSON_IsNumber(timestamp_item) ? (int64_t)timestamp_item->valuedouble : 0;
        HandleAdminMessage(content, device_id_str, messageType_str, sender_str, timestamp);
    } else {
        ESP_LOGI(TAG, "Unhandled message type: %s", type_str.c_str());
    }
}

void Esp32Livestream::HandleAdminMessage(const cJSON* content, const std::string& device_id, 
                                        const std::string& messageType, const std::string& sender, int64_t timestamp) {
    ESP_LOGI(TAG, "Admin message received: device_id=%s, messageType=%s, sender=%s, timestamp=%lld", 
             device_id.c_str(), messageType.c_str(), sender.c_str(), timestamp);
    
    if (messageType == "text") {
        // 处理text类型的消息（简化后的逻辑）
        if (cJSON_IsString(content)) {
            std::string wake_word = content->valuestring;
            ESP_LOGI(TAG, "Admin text message content: %s", wake_word.c_str());

            // 获取当前设备状态
            auto& app = Application::GetInstance();
            DeviceState device_state = app.GetDeviceState();
            
            // 根据设备状态执行相应操作
            if (device_state == kDeviceStateListening || device_state == kDeviceStateIdle) {
                ESP_LOGI(TAG, "Device state is %d, executing wake word action: %s", device_state, wake_word.c_str());
                
                if (device_state == kDeviceStateListening) {
                    // 在listening状态下，发送wake word detected消息
                    Protocol* protocol = &Application::GetInstance().GetProtocol();
                    protocol->SendWakeWordDetected(wake_word);
                    ESP_LOGI(TAG, "Sent wake word detected: %s", wake_word.c_str());
                } else if (device_state == kDeviceStateIdle) {
                    // 在idle状态下，直接调用唤醒词
                    app.WakeWordInvoke(wake_word);
                    ESP_LOGI(TAG, "Invoked wake word: %s", wake_word.c_str());
                }
            } else {
                ESP_LOGI(TAG, "Device state is %d, ignoring admin message: %s", device_state, wake_word.c_str());
            }
        }
    } else if (messageType == "control") {
        // 处理control类型的消息
        if (cJSON_IsString(content)) {
            std::string control_json_str = content->valuestring;
            // 限制日志输出长度为最多100个字符
            std::string truncated_content = control_json_str.length() > 100 ? 
                control_json_str.substr(0, 100) + "..." : control_json_str;
            ESP_LOGI(TAG, "Admin control message content: %s", truncated_content.c_str());
            
            // 解析control JSON字符串
            cJSON* control_json = cJSON_Parse(control_json_str.c_str());
            if (control_json) {
                cJSON* type_item = cJSON_GetObjectItem(control_json, "type");
                if (cJSON_IsString(type_item)) {
                    std::string control_type = type_item->valuestring;
                    ESP_LOGI(TAG, "Control type: %s", control_type.c_str());
                    
                    if (control_type == "audio_speaker_volume") {
                        HandleVolumeControl(control_json);
                    } else if (control_type == "screen_brightness") {
                        HandleBrightnessControl(control_json);
                    } else {
                        ESP_LOGW(TAG, "Unknown control type: %s", control_type.c_str());
                    }
                } else {
                    ESP_LOGE(TAG, "Control message missing 'type' field");
                }
                cJSON_Delete(control_json);
            } else {
                ESP_LOGE(TAG, "Failed to parse control JSON: %s", control_json_str.c_str());
            }
        } else {
            ESP_LOGE(TAG, "Control message content is not a string");
        }
    } else {
        ESP_LOGW(TAG, "Unknown messageType: %s", messageType.c_str());
    }
}

bool Esp32Livestream::StartReconnectTimer() {
    if (is_reconnecting_) {
        ESP_LOGW(TAG, "Reconnect timer is already running");
        return true;
    }
    
    ESP_LOGI(TAG, "Starting reconnect timer (attempt %d/%d)...", 
             reconnect_attempts_ + 1, MAX_RECONNECT_ATTEMPTS);
    
    // 如果达到最大重连次数，停止重连（0表示无限重连）
    if (MAX_RECONNECT_ATTEMPTS > 0 && reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        ESP_LOGE(TAG, "Maximum reconnect attempts (%d) reached, giving up", MAX_RECONNECT_ATTEMPTS);
        return false;
    }
    
    // 创建重连定时器配置
    esp_timer_create_args_t timer_args = {
        .callback = ReconnectTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reconnect_timer"
    };
    
    // 创建定时器
    esp_err_t ret = esp_timer_create(&timer_args, &reconnect_timer_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create reconnect timer: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 启动定时器，延迟RECONNECT_INTERVAL_MS毫秒后触发
    ret = esp_timer_start_once(reconnect_timer_, RECONNECT_INTERVAL_MS * 1000);  // 转换为微秒
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start reconnect timer: %s", esp_err_to_name(ret));
        esp_timer_delete(reconnect_timer_);
        reconnect_timer_ = nullptr;
        return false;
    }
    
    is_reconnecting_ = true;
    ESP_LOGI(TAG, "Reconnect timer started successfully, will attempt reconnection in %d ms", RECONNECT_INTERVAL_MS);
    return true;
}

bool Esp32Livestream::StopReconnectTimer() {
    if (!is_reconnecting_) {
        ESP_LOGD(TAG, "Reconnect timer is not running");
        return true;
    }
    
    ESP_LOGI(TAG, "Stopping reconnect timer...");
    
    // 先设置状态为停止
    is_reconnecting_ = false;
    
    // 停止并删除定时器
    if (reconnect_timer_) {
        esp_err_t ret = esp_timer_stop(reconnect_timer_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to stop reconnect timer: %s", esp_err_to_name(ret));
        }
        
        ret = esp_timer_delete(reconnect_timer_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to delete reconnect timer: %s", esp_err_to_name(ret));
        }
        
        reconnect_timer_ = nullptr;
        ESP_LOGI(TAG, "Reconnect timer stopped and deleted");
    }
    
    ESP_LOGI(TAG, "Reconnect timer stopped successfully");
    return true;
}

void Esp32Livestream::AttemptReconnect() {
    ESP_LOGI(TAG, "Attempting to reconnect to livestream server (attempt %d)...", reconnect_attempts_ + 1);
    
    // 增加重连次数
    reconnect_attempts_++;
    
    // 停止重连定时器
    is_reconnecting_ = false;
    if (reconnect_timer_) {
        esp_timer_delete(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
    
    // 尝试重新连接
    if (ConnectToServer()) {
        ESP_LOGI(TAG, "Reconnection successful after %d attempts", reconnect_attempts_);
        // 重连成功，重置重连次数
        reconnect_attempts_ = 0;
        // 清除之前的错误信息
        last_error_.clear();
        
        // 更新显示状态
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus("服务器连接中");
    } else {
        ESP_LOGE(TAG, "Reconnection failed (attempt %d): %s", reconnect_attempts_, last_error_.c_str());
        
        // 更新显示状态
        auto display = Board::GetInstance().GetDisplay();
        std::string status_msg = "重连中(" + std::to_string(reconnect_attempts_) + ")";
        display->SetStatus(status_msg.c_str());
        
        // 如果未达到最大重连次数，继续尝试重连
        if (MAX_RECONNECT_ATTEMPTS == 0 || reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
            ESP_LOGI(TAG, "Scheduling next reconnection attempt...");
            StartReconnectTimer();
        } else {
            ESP_LOGE(TAG, "Maximum reconnect attempts reached, giving up");
            display->SetStatus("连接失败");
        }
    }
}

void Esp32Livestream::ReconnectTimerCallback(void* arg) {
    Esp32Livestream* self = static_cast<Esp32Livestream*>(arg);
    
    // 检查重连是否已停止
    if (!self->is_reconnecting_) {
        ESP_LOGD(TAG, "Reconnection stopped, ignoring timer callback");
        return;
    }
    
    // 尝试重连
    self->AttemptReconnect();
}

void Esp32Livestream::HandleVolumeControl(const cJSON* control_json) {
    cJSON* data_item = cJSON_GetObjectItem(control_json, "data");
    if (cJSON_IsNumber(data_item)) {
        int volume = data_item->valueint;
        if (volume >= 0 && volume <= 100) {
            auto& board = Board::GetInstance();
            auto codec = board.GetAudioCodec();
            if (codec) {
                codec->SetOutputVolume(volume);
                ESP_LOGI(TAG, "Audio speaker volume set to %d", volume);
                
                // 发送成功消息给服务端
                if (protocol_ && protocol_->IsAudioChannelOpened()) {
                    protocol_->SendDeviceStatusUpdate("audio_speaker_volume", volume, true);
                }
            } else {
                ESP_LOGE(TAG, "Audio codec not available");
                if (protocol_ && protocol_->IsAudioChannelOpened()) {
                    protocol_->SendDeviceStatusUpdate("audio_speaker_volume", volume, false);
                }
            }
        } else {
            ESP_LOGE(TAG, "Invalid volume value: %d (must be 0-100)", volume);
            if (protocol_ && protocol_->IsAudioChannelOpened()) {
                protocol_->SendDeviceStatusUpdate("audio_speaker_volume", volume, false);
            }
        }
    } else {
        ESP_LOGE(TAG, "Audio speaker volume control missing or invalid 'data' field");
    }
}

void Esp32Livestream::HandleBrightnessControl(const cJSON* control_json) {
    cJSON* data_item = cJSON_GetObjectItem(control_json, "data");
    if (cJSON_IsNumber(data_item)) {
        int brightness = data_item->valueint;
        if (brightness >= 0 && brightness <= 100) {
            auto& board = Board::GetInstance();
            auto backlight = board.GetBacklight();
            if (backlight) {
                backlight->SetBrightness(static_cast<uint8_t>(brightness), true);
                ESP_LOGI(TAG, "Screen brightness set to %d", brightness);
                
                // 发送成功消息给服务端
                if (protocol_ && protocol_->IsAudioChannelOpened()) {
                    protocol_->SendDeviceStatusUpdate("screen_brightness", brightness, true);
                }
            } else {
                ESP_LOGE(TAG, "Backlight not available");
                if (protocol_ && protocol_->IsAudioChannelOpened()) {
                    protocol_->SendDeviceStatusUpdate("screen_brightness", brightness, false);
                }
            }
        } else {
            ESP_LOGE(TAG, "Invalid brightness value: %d (must be 0-100)", brightness);
            if (protocol_ && protocol_->IsAudioChannelOpened()) {
                protocol_->SendDeviceStatusUpdate("screen_brightness", brightness, false);
            }
        }
    } else {
        ESP_LOGE(TAG, "Screen brightness control missing or invalid 'data' field");
    }
}

