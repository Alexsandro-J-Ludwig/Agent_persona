
#include "audioPlayer.h"

#include <iostream>
#include <limits>

// ============================================================
// CONSTRUTOR / DESTRUTOR
// ============================================================

AudioPlayer::AudioPlayer() = default;

AudioPlayer::~AudioPlayer()
{
    close();
}

// ============================================================
// INICIALIZAÇÃO
// ============================================================

bool AudioPlayer::initialize(double rate, int channelCount)
{
    if (initialized)
        close();

    if (rate <= 0 || channelCount <= 0)
        return false;

    PaError err = Pa_Initialize();

    if (err != paNoError)
    {
        std::cerr << "[AudioPlayer] Pa_Initialize: "
                  << Pa_GetErrorText(err) << '\n';
        return false;
    }

    sampleRate = rate;
    channels = channelCount;
    initialized = true;

    // O stream será aberto no primeiro play(),
    // quando soubermos o formato PCM.
    return true;
}

// ============================================================
// ABERTURA DO STREAM
// ============================================================

bool AudioPlayer::openStream(PaSampleFormat format)
{
    if (!initialized)
        return false;

    // Se o formato mudou, precisamos recriar o stream.
    if (stream != nullptr)
    {
        if (sampleFormat == format)
            return true;

        PaError err = Pa_CloseStream(stream);
        stream = nullptr;

        if (err != paNoError)
        {
            std::cerr << "[AudioPlayer] Pa_CloseStream: "
                      << Pa_GetErrorText(err) << '\n';
            return false;
        }
    }

    PaError err = Pa_OpenDefaultStream(
        &stream,
        0,        // Canais de entrada: nenhum
        channels, // Canais de saída
        format,   // paFloat32 ou paInt16
        sampleRate,
        256,     // Frames por buffer
        nullptr, // Callback: nenhum
        nullptr  // Dados do usuário
    );

    if (err != paNoError)
    {
        stream = nullptr;

        std::cerr << "[AudioPlayer] Pa_OpenDefaultStream: "
                  << Pa_GetErrorText(err) << '\n';
        return false;
    }

    sampleFormat = format;

    return true;
}

// ============================================================
// REPRODUÇÃO FLOAT32
// ============================================================

bool AudioPlayer::play(const std::vector<float> &buffer)
{
    if (!initialized || buffer.empty())
        return false;

    if (buffer.size() % static_cast<size_t>(channels) != 0)
        return false;

    if (!openStream(paFloat32))
        return false;

    PaError err = Pa_IsStreamStopped(stream);

    if (err < 0)
        return false;

    if (err == 1)
    {
        err = Pa_StartStream(stream);

        if (err != paNoError)
        {
            std::cerr << "[AudioPlayer] Pa_StartStream: "
                      << Pa_GetErrorText(err) << '\n';
            return false;
        }
    }

    const size_t frameCount =
        buffer.size() / static_cast<size_t>(channels);

    if (frameCount > std::numeric_limits<unsigned long>::max())
        return false;

    err = Pa_WriteStream(
        stream,
        buffer.data(),
        static_cast<unsigned long>(frameCount));

    if (err != paNoError && err != paOutputUnderflowed)
    {
        std::cerr << "[AudioPlayer] Pa_WriteStream: "
                  << Pa_GetErrorText(err) << '\n';
        return false;
    }

    return true;
}

// ============================================================
// REPRODUÇÃO INT16
// ============================================================

bool AudioPlayer::play(const std::vector<int16_t> &buffer)
{
    if (!initialized || buffer.empty())
        return false;

    if (buffer.size() % static_cast<size_t>(channels) != 0)
        return false;

    if (!openStream(paInt16))
        return false;

    PaError err = Pa_IsStreamStopped(stream);

    if (err < 0)
        return false;

    if (err == 1)
    {
        err = Pa_StartStream(stream);

        if (err != paNoError)
        {
            std::cerr << "[AudioPlayer] Pa_StartStream: "
                      << Pa_GetErrorText(err) << '\n';
            return false;
        }
    }

    const size_t frameCount =
        buffer.size() / static_cast<size_t>(channels);

    if (frameCount > std::numeric_limits<unsigned long>::max())
        return false;

    err = Pa_WriteStream(
        stream,
        buffer.data(),
        static_cast<unsigned long>(frameCount));

    if (err != paNoError && err != paOutputUnderflowed)
    {
        std::cerr << "[AudioPlayer] Pa_WriteStream: "
                  << Pa_GetErrorText(err) << '\n';
        return false;
    }

    return true;
}

// ============================================================
// PARAR REPRODUÇÃO
// ============================================================

void AudioPlayer::stop()
{
    if (!stream)
        return;

    PaError state = Pa_IsStreamStopped(stream);

    if (state == 0)
    {
        // Interrompe imediatamente e descarta o áudio pendente.
        PaError err = Pa_AbortStream(stream);

        if (err != paNoError)
        {
            std::cerr << "[AudioPlayer] Pa_AbortStream: "
                      << Pa_GetErrorText(err) << '\n';
        }
    }
}

// ============================================================
// LIBERAR RECURSOS
// ============================================================

void AudioPlayer::close()
{
    if (stream)
    {
        PaError state = Pa_IsStreamStopped(stream);

        if (state == 0)
        {
            // Aguarda a reprodução pendente terminar.
            PaError err = Pa_StopStream(stream);

            if (err != paNoError)
            {
                std::cerr << "[AudioPlayer] Pa_StopStream: "
                          << Pa_GetErrorText(err) << '\n';
            }
        }

        PaError err = Pa_CloseStream(stream);

        if (err != paNoError)
        {
            std::cerr << "[AudioPlayer] Pa_CloseStream: "
                      << Pa_GetErrorText(err) << '\n';
        }

        stream = nullptr;
    }

    if (initialized)
    {
        PaError err = Pa_Terminate();

        if (err != paNoError)
        {
            std::cerr << "[AudioPlayer] Pa_Terminate: "
                      << Pa_GetErrorText(err) << '\n';
        }

        initialized = false;
    }
}

// ============================================================
// ESTADO
// ============================================================

bool AudioPlayer::isInitialized() const
{
    return initialized;
}
