#include "audio.h"

#include <cstdio>
#include <iostream>

TTSService::TTSService()
{
    const Model &model = getModel(ModelRole::TTS);

    tts = qwen3_tts_create(
        model.name,
        model.tts.threads);

    if (!tts)
    {
        std::cerr
            << "Erro ao inicializar TTS: "
            << qwen3_tts_get_last_create_error()
            << '\n';

        return;
    }

    qwen3_tts_default_params(&params);

    params.max_audio_tokens = model.tts.max_audio_tokens;

    std::snprintf(
        params.language_name,
        sizeof(params.language_name),
        "%s",
        model.tts.language);

    std::snprintf(
        params.instruct,
        sizeof(params.instruct),
        "%s",
        model.tts.instruct);
}

TTSService::~TTSService()
{
    if (tts)
        qwen3_tts_destroy(tts);
}

bool TTSService::speak(const std::string &text)
{
    if (!tts || text.empty())
        return false;

    Qwen3TtsAudio *audio = qwen3_tts_synthesize(
        tts,
        text.c_str(),
        &params);

    if (!audio)
        return false;

    int result = qwen3_tts_play(
        audio->samples,
        audio->n_samples,
        audio->sample_rate);

    qwen3_tts_free_audio(audio);

    return result == 0;
}