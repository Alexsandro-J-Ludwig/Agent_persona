#include "models.h"

const std::array<Model, MODEL_COUNT> MODEL_LIST =
    {{{
          "Qwen3.5-9B:latest",
          ModelRole::Agent,
          ModelBackend::Ollama,
          16384,
          8192,
          30 * MINUTE,
      },
      {"Mixtral-8x7B:latest",
       ModelRole::Think,
       ModelBackend::Ollama,
       32768,
       16384,
       0}}};

const Model &getModel(ModelRole role)
{
    return MODEL_LIST.at(
        static_cast<std::size_t>(role));
}
