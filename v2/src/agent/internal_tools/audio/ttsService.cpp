#include "ttsService.h"
#include "platform.h"
#include "src/configs/models.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <mutex>

#ifndef _WIN32
#include <csignal>
#endif

TTSService::TTSService()
{
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN); // fwrite retorna erro em vez de matar o processo
#endif

    const Model &model = getModel(ModelRole::TTS);
    model_path = model.name ? model.name : "";
    sample_rate = model.tts.model_rate > 0 ? model.tts.model_rate : 22050;

    if (openStream())
        std::cerr << "[TTS] Piper iniciado (" << model_path << " @ " << sample_rate << " Hz)\n";
    else
        std::cerr << "[TTS ERRO] Nao foi possivel abrir o pipeline do Piper.\n";
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

    namespace fs = std::filesystem;
    const auto &cfg = model.tts;

    const fs::path exe = fs::absolute(platform::piperExecutable());
    if (!fs::exists(exe))
    {
        std::cerr << "[TTS ERRO] Piper nao encontrado em " << exe << '\n';
        return false;
    }

    char flags[256];
    std::snprintf(flags, sizeof(flags),
                  "--length_scale %.2f --noise_scale %.3f --noise_w %.2f --sentence_silence %.2f",
                  cfg.length_scale, cfg.noise_scale, cfg.noise_w, cfg.sentence_silence);

    // piper ... --output_raw | player
    const std::string cmd =
        platform::quote(exe) + " --model " + platform::quote(fs::absolute(model.name)) +
        " " + flags + " --output_raw | " + platform::playerCommand(sample_rate);

    std::cerr << "[TTS DEBUG] Comando: " << cmd << '\n';

    piper_stream = platform::openPipeWrite(cmd);
    return piper_stream != nullptr;
}

void TTSService::closeStream()
{
    if (piper_stream)
    {
        platform::closePipe(piper_stream);
        piper_stream = nullptr;
    }
}

bool TTSService::speak(const std::string &text)
{
    std::cerr << "[TTS DEBUG] speak() recebeu: " << text << '\n';

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
    // ====== CONFIRMA A ESCRITA E O FLUSH DO PIPE ======
    const int flushResult = std::fflush(piper_stream);

    std::cerr << "[TTS DEBUG] Bytes enviados: "
              << written << "/" << clean_text.size() << '\n';

    if (written != clean_text.size() || flushResult != 0 || std::ferror(piper_stream))
    {
        std::cerr << "[TTS AVISO] Pipe quebrou, tentando reconectar...\n";
        closeStream();
        if (openStream())
        {
            const auto retried = std::fwrite(clean_text.data(), 1, clean_text.size(), piper_stream);
            const int retryFlush = std::fflush(piper_stream);
            return retried == clean_text.size() && retryFlush == 0 && !std::ferror(piper_stream);
        }
        return false;
    }

    return true;
}
