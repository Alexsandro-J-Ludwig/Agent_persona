#ifndef TTSSERVICE_H
#define TTSSERVICE_H

#include <cstdio>
#include <mutex>
#include <string>

class TTSService
{
private:
    FILE *piper_stream = nullptr;
    std::mutex stream_mutex;
    std::string model_path;
    int sample_rate = 22050;

    bool openStream();
    void closeStream();

public:
    TTSService();
    ~TTSService();

    TTSService(const TTSService &) = delete;
    TTSService &operator=(const TTSService &) = delete;

    bool speak(const std::string &text);
};

#endif // TTSSERVICE_H