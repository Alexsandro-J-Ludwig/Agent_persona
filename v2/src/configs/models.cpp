#include "models.h"

const std::array<Model, MODEL_COUNT> MODEL_LIST =
    {{{
          "qwen3-30b-a3b:latest",
          ModelRole::Agent,
          ModelBackend::Ollama,
          32768,
          16384,
          30 * MINUTE,
      },
      {"qwen3-next-80b-a3b-thinking:latest",
       ModelRole::Think,
       ModelBackend::Ollama,
       32768,
       16384,
       0},
      {"ViewNew-TTS:latest",
       ModelRole::TTS,
       ModelBackend::TTSService,
       0,
       4092,
       0},
      {"LFM2-5-audio:latest",
       ModelRole::Audio,
       ModelBackend::AudioService,
       0,
       4092,
       0}}};

const Model &getModel(ModelRole role)
{
    return MODEL_LIST.at(
        static_cast<std::size_t>(role));
}
