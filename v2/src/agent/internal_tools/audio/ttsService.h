#ifndef TTSSERVICE_H
#define TTSSERVICE_H

#include "../libs/qwen3-tts/tts-package/include/qwen3tts_c_api.h"

#include <string>
#include <vector>

class TTSService
{
private:
    Qwen3Tts *tts = nullptr;
    Qwen3TtsParams params{};

public:
    TTSService();
    ~TTSService();

    TTSService(const TTSService &) = delete;
    TTSService &operator=(const TTSService &) = delete;

    bool speak(const std::string &text);
};

#endif