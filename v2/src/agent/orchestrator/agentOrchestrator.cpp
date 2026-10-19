#include "agentOrchestrator.h"

Orquestrador::Orquestrador()
    : manager(tools)
{
}

std::tuple<ModelRole, bool, bool> Orquestrador::selectModel(
    const std::string &prompt)
{
    const auto result = router.route(prompt);

    setAudioEnabled(std::get<2>(result));

    return {std::get<0>(result), std::get<1>(result), std::get<2>(result)};
}

std::string Orquestrador::run(
    const std::string &prompt,
    ModelRole role,
    bool thinking,
    bool audio,
    Ollama::StreamCallback onChunk)
{
    const Model &model = getModel(role);

    // Altera o system prompt com base no modelo selecionado em router
    if (manager.getHistory().empty())
        manager.createHistory(role, thinking);
    else
        manager.alterSystemHistory(role, thinking);

    // Adiciona o prompt do usuario no historico
    manager.addHistory("user", prompt);

    std::string response = ollama.chatStream(
        model,
        manager.getHistory(),
        onChunk,
        thinking);

    // Adiciona a resposta da IA no historico
    manager.addHistory("assistant", response);

    if (audio)
    {
        tools.executeSystemTool("audioPlayer", response);
    }

    return response;
}

void Orquestrador::setAudioEnabled(bool enabled)
{
    if (enabled)
        ttsService.start(
            L"v2\\src\\services\\tts\\server.exe");
    else
        ttsService.stop();
}