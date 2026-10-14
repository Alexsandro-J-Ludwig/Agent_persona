#include "agentOrchestrator.h"
#include <tuple>

Orquestrador::Orquestrador()
{
}

std::pair<ModelRole, bool> Orquestrador::selectModel(
    const std::string &prompt)
{
    const auto result = router.route(prompt);
    return {std::get<0>(result), std::get<1>(result)};
}

std::string Orquestrador::run(
    const std::string &prompt,
    ModelRole role,
    bool thinking,
    Ollama::StreamCallback onChunk)
{
    const Model &model = getModel(role);

    if (manager.getHistory().empty())
        manager.createHistory(role, thinking);
    else
        manager.alterSystemHistory(role, thinking);

    manager.addHistory("user", prompt);

    std::string response = ollama.chatStream(
        model,
        manager.getHistory(),
        onChunk,
        thinking);

    manager.addHistory("assistant", response);

    return response;
}