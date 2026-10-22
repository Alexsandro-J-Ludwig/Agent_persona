#ifndef MODELS_H
#define MODELS_H

#include <array>
#include <cstddef>
#include <string>

/*
Declaração de uso dos modelos, atual configuração:
    - Agent: vai realizar todas as tarefas que não exigem raciocinio pesado.
    - Think: especialista para criação de tarefas que envovlem multiplas etapas ou pensamento complexo.
    - Voice: modelo de conversão de texto para voz

Esse header prepara o sistema para usar os modelos disponiveis em formato de lista.
Tal escolha é resultado de uma escolhe deliberada de uso de arquitetura e modelos. Além de
melhorar a organização e arquitetura do código, como é possível ver nas pastas contendo projetos em python
*/
enum class ModelBackend
{
    Ollama,
    TTSService
};

enum class ModelRole : std::size_t
{
    Agent,
    Think,
    TTS,

    Count
};

struct TTSConfig
{
    int model_rate = 22050;        // Taxa original do modelo .onnx (22050 Hz)
    int playback_rate = 22050;     // Mude para 19500 (mais grave) ou 24000 (mais agudo)
    float length_scale = 0.90f;    // Velocidade: < 1.0 mais rápido | > 1.0 mais lento
    float noise_scale = 0.667f;    // Entonação/Emoção: 0.4 mais sério/firme | 0.8 mais expressivo
    float noise_w = 0.8f;          // Cadência dos fonemas (padrão 0.8)
    float sentence_silence = 0.05f;// Pausa entre um chunk e outro (em segundos! Padrão é 0.2s, 0.05s tira a pausa robótica)
};

struct Model
{
    const char *name;

    ModelRole role;
    ModelBackend backend;

    std::size_t num_ctx;
    std::size_t num_predict;

    int keep_alive;

    TTSConfig tts{};
};

constexpr std::size_t MODEL_COUNT =
    static_cast<std::size_t>(ModelRole::Count);

constexpr int MINUTE = 60;

extern const std::array<Model, MODEL_COUNT>
    MODEL_LIST;

const Model &getModel(ModelRole role);

#endif // MODELS_H