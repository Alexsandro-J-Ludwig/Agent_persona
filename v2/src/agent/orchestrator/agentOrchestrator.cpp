#include "agentOrchestrator.h"
#include <stdexcept>

Orquestrador::Orquestrador()
    : router(tools),
      manager(tools),
      taskManager(tools),
      speech([this](const std::string &phrase)
             {
          auto future = tools.execute(
              ToolCaller::System,
              "audioPlayer",
              phrase
          );

          ToolResult result = future.get();

          if (!result.success)
              throw std::runtime_error(result.error); })
{
}

std::tuple<ModelRole, bool, bool, std::string> Orquestrador::selectModel(
    const std::string &prompt)
{
    const auto result = router.route(prompt);

    setAudioEnabled(std::get<2>(result));

    return {std::get<0>(result), std::get<1>(result), std::get<2>(result), std::get<3>(result)};
}

std::string Orquestrador::run(
    const std::string &prompt,
    ModelRole role,
    bool thinking,
    bool audio,
    Ollama::StreamCallback onChunk,
    ToolCallback onTool)
{
    const Model &model = getModel(role);

    // Altera o system prompt com base no modelo selecionado em router
    if (manager.getHistory().empty())
        manager.createHistory(role, thinking);

    // Adiciona o prompt do usuario no historico
    manager.compressIfNeeded(role);
    manager.addHistory("user", prompt);

    ChatResult response;
    nlohmann::json toolSchemas = tools.getSchema();

    for (int round = 0;; round++)
    {
        const bool toolsAllowed =
            round < Config::Properties::max_rounds;

        manager.alterSystemHistory(role, thinking, toolsAllowed);

        response = ollama.chatStream(
            model,
            manager.getHistory(),
            [&](const std::string &chunk)
            {
                onChunk(chunk); // Atualiza o terminal.

                if (audio)
                {
                    speech.feed(chunk);
                }
            },
            thinking,
            toolsAllowed ? toolSchemas : nlohmann::json::array());

        if (audio)
            speech.flush();

        manager.addHistory("assistant", response.response, response.toolCalls);

        if (response.toolCalls.empty() || !toolsAllowed)
            break;

        taskManager.toolCalling(response.toolCalls,
                                [&](const std::string &name, const ToolResult &r)
                                {
                                    manager.addHistory(
                                        "tool",
                                        r.success ? r.output : r.error,
                                        Ollama::json::array(),
                                        name);
                                    if (onTool)
                                        onTool(name, r);
                                });
    }

    return response.response;
}

void Orquestrador::setAudioEnabled(bool enabled)
{
    if (!enabled)
    {
        speech.cancelPending();
        speech.wait();
    }
}
