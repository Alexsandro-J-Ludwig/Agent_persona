#include "agentOrchestrator.h"
#include <stdexcept>

Orquestrador::Orquestrador()
    : router(tools),
      manager(tools),
      speech(
          [this](const std::string &phrase)
          {
              auto future = tools.execute(
                  ToolCaller::System,
                  "audioPlayer",
                  phrase);

              ToolResult result = future.get();

              if (!result.success)
                  throw std::runtime_error(result.error);
          },
          [this]()
          {
              auto future = tools.execute(
                  ToolCaller::System,
                  "stopAudio",
                  "");

              ToolResult result = future.get();

              if (!result.success)
                  throw std::runtime_error(result.error);
          }),
      taskManager(tools)
{
}

void Orquestrador::interruptSpeech() { speech.interrupt(); }

std::tuple<ModelRole, bool, bool, std::string> Orquestrador::selectModel(
    const std::string &prompt)
{
    const auto result = router.route(prompt);

    setAudioEnabled(std::get<2>(result));

    return {std::get<0>(result), std::get<1>(result), std::get<2>(result), std::get<3>(result)};
}

bool Orquestrador::submit(const std::string &msg)
{
    std::lock_guard lock(pendingMutex);
    if (busy)
    {
        pending.push_back(msg);
        return false;
    }
    busy = true;
    return true;
}

std::vector<std::string> Orquestrador::drainPending()
{
    std::lock_guard lock(pendingMutex);
    std::vector<std::string> out(pending.begin(), pending.end());
    pending.clear();
    return out;
}

bool Orquestrador::hasPendingOrRelease()
{
    std::lock_guard lock(pendingMutex);
    if (!pending.empty())
        return true;
    busy = false;
    return false;
}

void Orquestrador::release()
{
    std::lock_guard lock(pendingMutex);
    busy = false;
    pending.clear();
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
    manager.stripOldThinking();
    if (!prompt.empty())
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
                "system",
                "Limite de rounds atingido. Você não poderá chamar "
                "novas ferramentas nesta requisição.");
        }

        manager.alterSystemHistory(role, thinking, toolsAllowed);
        manager.audioIsActive(audio);

        for (const auto &msg : drainPending())
            manager.addHistory("user", msg);

        speech.resume();

        auto onModelChunk = [&](const std::string &chunk)
        {
            if (onChunk)
                onChunk(chunk);
            if (audio)
                speech.feed(chunk);
        };

        auto onModelThink = [&](const std::string &chunk)
        {
            if (thinkChunk)
                thinkChunk(chunk);
        };

        const nlohmann::json activeTools =
            toolsAllowed ? toolSchemas : nlohmann::json::array();

        response = ollama.chatStream(
            model, manager.getHistory(),
            onModelChunk, onModelThink,
            thinking, activeTools);

        if (audio)
            speech.flush();

        // Rodada inválida: vazia ou com tool call vazada como texto.
        const bool leaked =
            response.think.find("<rfl:parameter") != std::string::npos ||
            response.response.find("<rfl:parameter") != std::string::npos;

        if (response.toolCalls.empty() && (response.response.empty() || leaked))
        {
            std::cerr << "[RUN] rodada invalida, repetindo sem thinking\n";

            response = ollama.chatStream(
                model, manager.getHistory(),
                onModelChunk, onModelThink,
                false, activeTools);

            if (audio)
                speech.flush();
        }

        manager.addHistory(
            "assistant",
            response.response,
            response.toolCalls,
            "",
            response.think);

        if (response.toolCalls.empty() || !toolsAllowed)
            break;

        taskManager.toolCalling(
            response.toolCalls,
            [&](const std::string &name, const ToolResult &r)
            {
                manager.addHistory(
                    "tool",
                    r.success ? r.output
                              : "Falha ao executar a ferramenta: " + r.error,
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

// ====== REPASSA O OBSERVADOR SEM CONHECER O TERMINAL ======
void Orquestrador::setSpeechCallback(Speach::Speaker callback)
{
    speech.setPhraseCallback(std::move(callback));
}
