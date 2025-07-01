#ifndef ESP32_VOICE_CHANGER_H
#define ESP32_VOICE_CHANGER_H

#include <string>
#include "voice_changer.h"

#define TAG_VOICE_CHANGER "Esp32VoiceChanger"

class Esp32VoiceChanger : public VoiceChanger {
private:
    std::string current_voice_;

public:
    Esp32VoiceChanger();
    ~Esp32VoiceChanger();

    virtual bool SetVoice(const std::string& voice_type) override;
    virtual std::string GetCurrentVoice() override;
    virtual std::string GetAvailableVoices() override;
};

#endif // ESP32_VOICE_CHANGER_H 