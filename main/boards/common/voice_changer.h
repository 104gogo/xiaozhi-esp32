#ifndef VOICE_CHANGER_H
#define VOICE_CHANGER_H

#include <string>

class VoiceChanger {
public:
    virtual ~VoiceChanger() = default;
    virtual bool SetVoice(const std::string& voice_type) = 0;
    virtual std::string GetCurrentVoice() = 0;
    virtual std::string GetAvailableVoices() = 0;
};

#endif // VOICE_CHANGER_H 