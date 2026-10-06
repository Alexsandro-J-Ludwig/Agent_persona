#include "agentOrchestrator.h"

void Orquestrador::run(
    const std::string &prompt,
    Ollama::StreamCallback onChunk)
{
    auto [role, thinking] = router.route(prompt);
    const Model &model = getModel(role);

    manager.addHistory("user", prompt);

    const auto &history = manager.getHistory();

    std::string response = ollama.chatStream(
        model,
        history,
        onChunk,
        thinking);

    manager.addHistory("assistant", response);
}