#include "esp32_voice_changer.h"
#include "mcp_server.h"
#include "board.h"
#include "system_info.h"

#include <esp_log.h>
#include <cstring>

#define TAG "Esp32VoiceChanger"

Esp32VoiceChanger::Esp32VoiceChanger() {
    current_voice_ = "default";
    ESP_LOGI(TAG, "Esp32VoiceChanger initialized");
}

Esp32VoiceChanger::~Esp32VoiceChanger() {
    ESP_LOGI(TAG, "Esp32VoiceChanger destroyed");
}

bool Esp32VoiceChanger::SetVoice(const std::string& voice_type) {
    ESP_LOGI(TAG, "Setting voice to: %s", voice_type.c_str());
    // TODO: 这里将来会实现具体的音色切换逻辑
    current_voice_ = voice_type;
    return true;
}

std::string Esp32VoiceChanger::GetCurrentVoice() {
    return current_voice_;
}

std::string Esp32VoiceChanger::GetAvailableVoices() {
    // TODO: 这里将来会返回实际可用的音色列表
    return R"(["default", "male", "female", "child", "elder"])";
} 