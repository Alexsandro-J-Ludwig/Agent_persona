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
    TTSService,
};

enum class ModelRole : std::size_t
{
    Agent,
    Think,
    TTS,

    Count
};

struct Model
{
    const char *name;

    ModelRole role;
    ModelBackend backend;

    std::size_t num_ctx;
    int keep_alive;
};

constexpr std::size_t MODEL_COUNT =
    static_cast<std::size_t>(ModelRole::Count);

constexpr int MINUTE = 60;

extern const std::array<Model, MODEL_COUNT>
    MODEL_LIST;

const Model &getModel(ModelRole role);

#endif // MODELS_H