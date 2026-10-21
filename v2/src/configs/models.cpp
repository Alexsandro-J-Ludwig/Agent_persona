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
      {"libs/qwen3-tts/tts-package/models/qwen3-tts-0.6b-customvoice-q8_0.gguf",
       ModelRole::TTS,
       ModelBackend::TTSService,
       0,
       0,
       0,
       TTSConfig{
           "portuguese", // Language
           "",           // Speaker
           "",           // Instruct
           4,            // Threads
           1024          // Max audio tokens
       }}

    }};

const Model &
getModel(ModelRole role)
{
    return MODEL_LIST.at(
        static_cast<std::size_t>(role));
}
