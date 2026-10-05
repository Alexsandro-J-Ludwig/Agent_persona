#include "models.h"

const std::array<Model, MODEL_COUNT> MODEL_LIST =
    {{{
          "gemma4:e4b",
          ModelRole::Agent,
          ModelBackend::Ollama,
          16384,
          30 * MINUTE,
      },
      {"qwen3-next-80b-a3b-thinking:latest",
       ModelRole::Think,
       ModelBackend::Ollama,
       16384,
       0},
      {"kokoro-82m",
       ModelRole::TTS,
       ModelBackend::TTSService,
       0,
       0}}};

const Model &getModel(ModelRole role)
{
    return MODEL_LIST.at(
        static_cast<std::size_t>(role));
}