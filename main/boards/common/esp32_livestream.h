#ifndef ESP32_LIVESTREAM_H
#define ESP32_LIVESTREAM_H

#include "../protocols/protocol.h"
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

class WebsocketProtocol;
class OpusDecoderWrapper;
class OpusResampler;

// 前置声明
struct cJSON;

class Esp32Livestream {
private:
    bool connected_ = false;
    std::unique_ptr<WebsocketProtocol> protocol_;
    std::string last_error_;
    
    // 服务器IP地址 - 硬编码
    static constexpr const char* SERVER_IP = "your-ip-address";
    
    // 端口定义
    static constexpr int LIVESTREAM_PORT = 8200;

    // 重连相关
    esp_timer_handle_t reconnect_timer_ = nullptr;
    bool is_reconnecting_ = false;
    int reconnect_attempts_ = 0;
    static const int RECONNECT_INTERVAL_MS = 5000; // 5秒重连间隔
    static const int MAX_RECONNECT_ATTEMPTS = 10; // 最大重连次数，0表示无限重连
    
public:
    Esp32Livestream();
    virtual ~Esp32Livestream();
    
    // 核心功能接口
    bool IsConnected();
    std::string GetStatus();
    bool ConnectToServer();
    void DisconnectFromServer();
    
    // 重连相关方法
    bool StartReconnectTimer();
    bool StopReconnectTimer();
    void AttemptReconnect();
    
private:
    // URL构建辅助方法
    std::string GetLivestreamServerUrl() const;
    
    // WebSocket协议处理方法
    void SendLivestreamHello();
    void HandleIncomingJson(const cJSON* root);
    
    // 消息处理方法
    void HandleAdminMessage(const cJSON* content, const std::string& device_id, 
                           const std::string& messageType, const std::string& sender, int64_t timestamp);
    
    // 控制功能处理方法
    void HandleVolumeControl(const cJSON* control_json);
    void HandleBrightnessControl(const cJSON* control_json);
    
    // 定时器回调方法
    static void ReconnectTimerCallback(void* arg);
};

#endif // ESP32_LIVESTREAM_H 