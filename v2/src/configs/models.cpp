#include "models.h"

const std::array<Model, MODEL_COUNT> MODEL_LIST =
    /*
        // =========== LOCAL =================
        {{{
              "Qwen3.5-9B:latest",
              ModelRole::Agent,
              ModelBackend::LlmServer,
              32768,
              16384,
              30 * MINUTE,
          },
          {"Qwen3.8-27B:latest",
           ModelRole::Think,
           ModelBackend::LlmServer,
           32768,
           16384,
           0},
          */
    // ============ REMOTE ==================
    {{{
          "qwen3-30b-a3b:latest",
          ModelRole::Agent,
          ModelBackend::LlmServer,
          32768, // Depracted
          16384,
          30 * MINUTE, // Depracted
      },
      {"qwen3-next-80b-a3b-thinking:latest",
       ModelRole::Think,
       ModelBackend::LlmServer,
       32768,
       16384,
       0},

      {"libs/piper/piper-models/pt_BR-faber-medium.onnx",
       ModelRole::TTS,
       ModelBackend::TTSService,
       0,
       0,
       0,
       TTSConfig{
           22050,  // model_rate
           22050,  // playback_rate (21000 deixa a voz levemente mais grave/encorpada que 22050)
           1.00f,  // length_scale (12% mais rápido para compensar o tom mais grave)
           0.667f, // noise_scale (fala mais firme e menos cantada)
           0.8f,   // noise_w
           0.15f   // sentence_silence (apenas 50ms de pausa entre frases para não ficar robótico!)
       }}}};

const Model &
getModel(ModelRole role)
{
    return MODEL_LIST.at(
        static_cast<std::size_t>(role));
}
