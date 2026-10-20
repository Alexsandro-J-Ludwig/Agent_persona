#pragma once

#include <string>
#include <windows.h>

class TTSService
{
public:
    TTSService() = default;
    ~TTSService();

    TTSService(const TTSService &) = delete;
    TTSService &operator=(const TTSService &) = delete;

    bool start(const std::wstring &exePath);
    void stop();

private:
    PROCESS_INFORMATION process{};
};
