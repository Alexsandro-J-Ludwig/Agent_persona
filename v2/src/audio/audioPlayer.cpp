
#include "audioPlayer.h"

#include <iostream>
#include <limits>
#include <algorithm>
#include <cstring>
#include <memory>
#include <sndfile.h>

namespace
{
struct WavMemory
{
    const std::string &bytes;
    sf_count_t position = 0;
};

sf_count_t wavLength(void *user)
{
    return static_cast<sf_count_t>(static_cast<WavMemory *>(user)->bytes.size());
}

sf_count_t wavSeek(sf_count_t offset, int origin, void *user)
{
    auto &memory = *static_cast<WavMemory *>(user);
    const sf_count_t length = wavLength(user);
    sf_count_t base;
    switch (origin)
    {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = memory.position; break;
    case SEEK_END: base = length; break;
    default: return -1;
    }
    if (offset < -base || offset > length - base)
        return -1;
    memory.position = base + offset;
    return memory.position;
}

sf_count_t wavRead(void *destination, sf_count_t count, void *user)
{
    auto &memory = *static_cast<WavMemory *>(user);
    if (count <= 0) return 0;
    count = std::min(count, wavLength(user) - memory.position);
    if (count > 0)
        std::memcpy(destination, memory.bytes.data() + memory.position,
                    static_cast<size_t>(count));
    memory.position += count;
    return count;
}

sf_count_t wavWrite(const void *, sf_count_t, void *) { return 0; }
sf_count_t wavTell(void *user)
{
    return static_cast<WavMemory *>(user)->position;
}
}

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

bool AudioPlayer::playWav(const std::string &wav)
{
    if (wav.empty() || wav.size() > static_cast<size_t>(std::numeric_limits<sf_count_t>::max()))
        return false;

    WavMemory memory{wav};
    SF_VIRTUAL_IO wavVirtualIO{wavLength, wavSeek, wavRead, wavWrite, wavTell};
    SF_INFO info{};

    std::unique_ptr<SNDFILE, decltype(&sf_close)> file(sf_open_virtual(
        &wavVirtualIO,
        SFM_READ,
        &info,
        &memory), &sf_close);

    if (!file)
        return false;

    if ((info.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
        info.frames <= 0 || info.channels <= 0 || info.samplerate <= 0 ||
        static_cast<unsigned long long>(info.frames) >
            std::vector<int16_t>().max_size() / static_cast<size_t>(info.channels))
        return false;

    std::vector<int16_t> samples(
        static_cast<size_t>(info.frames) * info.channels);

    sf_count_t frames = sf_readf_short(
        file.get(),
        samples.data(),
        info.frames);

    file.reset();

    if (frames <= 0)
        return false;

    samples.resize(
        static_cast<size_t>(frames) * info.channels);

    if (!initialize(info.samplerate, info.channels))
        return false;

    return play(samples);
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
