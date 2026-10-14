#include "models.h"

const std::array<Model, MODEL_COUNT> MODEL_LIST =
    {{{
          "qwen3-agent:latest",
          ModelRole::Agent,
          ModelBackend::Ollama,
          16384,
          8192,
          30 * MINUTE,
      },
      {"qwen3.6-35B-A3B:latest",
       ModelRole::Think,
       ModelBackend::Ollama,
       16384,
       8192,
       0},
      {"ViewNew-TTS:latest",
       ModelRole::TTS,
       ModelBackend::TTSService,
       0,
       4092,
       0},
    {
        "LFM2-5-audio:latest",
        ModelRole::Audio,
        ModelBackend::AudioService,
        0,
        4092,
        0
    }}};

const Model &getModel(ModelRole role)
{
    return MODEL_LIST.at(
        static_cast<std::size_t>(role));
}