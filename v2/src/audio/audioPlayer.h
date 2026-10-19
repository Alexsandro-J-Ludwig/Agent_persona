#pragma once

#include <portaudio.h>
#include <vector>
#include <cstdint>
#include <string>

class AudioPlayer
{
public:
    AudioPlayer();
    ~AudioPlayer();

    // Inicializa o dispositivo de saída.
    bool initialize(
        double sampleRate = 24000.0,
        int channels = 1);

    bool playWav(const std::string &wav);

    // Reproduz um buffer PCM float32.
    bool play(const std::vector<float> &buffer);

    // Reproduz um buffer PCM int16.
    bool play(const std::vector<int16_t> &buffer);

    // Interrompe a reprodução.
    void stop();

    // Libera o dispositivo de áudio.
    void close();

    // Informa se o dispositivo foi inicializado.
    bool isInitialized() const;

private:
    PaStream *stream = nullptr;

    double sampleRate = 24000.0;
    int channels = 1;

    PaSampleFormat sampleFormat = paFloat32;

    bool initialized = false;

    bool openStream(PaSampleFormat format);
};
