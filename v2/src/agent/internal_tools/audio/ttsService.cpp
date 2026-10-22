#include "ttsService.h"
#include "src/configs/models.h"

#include <algorithm>
#include <iostream>

TTSService::TTSService()
{
    const Model &model = getModel(ModelRole::TTS);
    model_path = model.name ? model.name : "";
    sample_rate = model.tts.model_rate > 0 ? model.tts.model_rate : 22050;

    if (openStream())
    {
        std::cerr << "[TTS] Piper inicializado em modo streaming persistente ("
                  << model_path << " @ " << sample_rate << " Hz)\n";
    }
    else
    {
        std::cerr << "[TTS ERRO] Nao foi possivel abrir o pipeline do Piper.\n";
    }
}

TTSService::~TTSService()
{
    closeStream();
}

bool TTSService::openStream()
{
    const Model &model = getModel(ModelRole::TTS);
    if (!model.name || model.name[0] == '\0')
        return false;

    const auto &cfg = model.tts;

    // Força formatação com ponto decimal (ex: 0.88 em vez de 0,88 caso o locale do sistema esteja em PT-BR)
    char flags[256];
    std::snprintf(
        flags,
        sizeof(flags),
        "--length_scale %.2f --noise_scale %.3f --noise_w %.2f --sentence_silence %.2f",
        cfg.length_scale,
        cfg.noise_scale,
        cfg.noise_w,
        cfg.sentence_silence);

#ifdef _WIN32
    std::string cmd = "piper --model \"" + std::string(model.name) + "\" " + flags + " --output-raw 2>NUL | "
                      "ffplay -nodisp -autoexit -f s16le -ar " + std::to_string(cfg.playback_rate) + " -ac 1 - 2>NUL";
    piper_stream = _popen(cmd.c_str(), "w");
#else
    std::string cmd = "piper-tts --model \"" + std::string(model.name) + "\" " + flags + " --output-raw 2>/dev/null | "
                      "pacat --rate=" + std::to_string(cfg.playback_rate) + " --channels=1 --format=s16le --latency-msec=40 2>/dev/null";
    piper_stream = popen(cmd.c_str(), "w");
#endif

    std::cerr << "[TTS] Comando ativo: " << cmd << '\n';
    return (piper_stream != nullptr);
}

void TTSService::closeStream()
{
    if (piper_stream)
    {
#ifdef _WIN32
        _pclose(piper_stream);
#else
        pclose(piper_stream);
#endif
        piper_stream = nullptr;
    }
}

bool TTSService::speak(const std::string &text)
{
    if (text.empty())
        return false;

    std::lock_guard<std::mutex> lock(stream_mutex);

    if (!piper_stream && !openStream())
    {
        std::cerr << "[TTS ERRO] Stream fechado e falha ao reabrir.\n";
        return false;
    }

    std::string clean_text = text;
    std::replace(clean_text.begin(), clean_text.end(), '\n', ' ');
    std::replace(clean_text.begin(), clean_text.end(), '\r', ' ');

    if (clean_text.find_first_not_of(" \t") == std::string::npos)
        return true;

    clean_text += '\n';

    const std::size_t written = std::fwrite(clean_text.data(), 1, clean_text.size(), piper_stream);
    std::fflush(piper_stream);

    if (written != clean_text.size())
    {
        std::cerr << "[TTS AVISO] Pipe quebrou, tentando reconectar...\n";
        closeStream();
        if (openStream())
        {
            std::fwrite(clean_text.data(), 1, clean_text.size(), piper_stream);
            std::fflush(piper_stream);
            return true;
        }
        return false;
    }

    return true;
}