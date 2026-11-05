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
    Ollama::ThinkCallback thinkChunk,
    ToolCallback onTool)
{
    const Model &model = getModel(role);

    // Cria o histórico conforme o modelo selecionado.
    if (manager.getHistory().empty())
        manager.createHistory(role, thinking);

    manager.compressIfNeeded(role);
    manager.addHistory("user", prompt);

    ChatResult response;
    nlohmann::json toolSchemas = tools.getSchema();

    for (int round = 0;; round++)
    {
        const bool toolsAllowed =
            round < Config::Properties::max_rounds;

        if (round == Config::Properties::max_rounds)
        {
            manager.addHistory(
                "System",
                "Limite de rounds atingido. Você não poderá chamar "
                "novas ferramentas nesta requisição.");
        }

        manager.alterSystemHistory(role, thinking, toolsAllowed);

        response = ollama.chatStream(
            model,
            manager.getHistory(),

            // Resposta do modelo.
            [&](const std::string &chunk)
            {
                if (onChunk)
                    onChunk(chunk);

                if (audio)
                    speech.feed(chunk);
            },

            // Pensamento do modelo.
            [&](const std::string &chunk)
            {
                if (thinkChunk)
                    thinkChunk(chunk);
            },

            thinking,
            toolsAllowed
                ? toolSchemas
                : nlohmann::json::array());

        if (audio)
            speech.flush();

        manager.addHistory(
            "assistant",
            response.response,
            response.toolCalls);

        // Não existem mais ferramentas para executar.
        if (response.toolCalls.empty() || !toolsAllowed)
            break;

        // Executa as ferramentas e apenas notifica o observador.
        taskManager.toolCalling(
            response.toolCalls,

            [onTool](const std::string &name, const ToolResult &result)
            {
                if (onTool)
                    onTool(name, result);
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

// ====== REPASSA O OBSERVADOR SEM CONHECER O TERMINAL ======
void Orquestrador::setSpeechCallback(Speach::Speaker callback)
{
    speech.setPhraseCallback(std::move(callback));
}
