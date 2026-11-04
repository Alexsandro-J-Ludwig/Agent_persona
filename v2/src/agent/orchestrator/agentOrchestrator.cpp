#include "agentOrchestrator.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

Orquestrador::Orquestrador()
    : router(tools),
      manager(tools),
      speech([this](const std::string &phrase)
             {
                 auto future = tools.execute(
                     ToolCaller::System,
                     "audioPlayer",
                     phrase);

                 ToolResult result = future.get();

                 if (!result.success)
                     throw std::runtime_error(result.error); }),
      taskManager(tools)
{
}

// ===========================================================
// Seleciona modelo, pensamento e áudio
// ===========================================================

std::tuple<ModelRole, bool, bool, std::string>
Orquestrador::selectModel(const std::string &prompt)
{
    const auto result = router.route(prompt);

    setAudioEnabled(std::get<2>(result));

    return {
        std::get<0>(result),
        std::get<1>(result),
        std::get<2>(result),
        std::get<3>(result)};
}

// ===========================================================
// Execução principal do agente
// ===========================================================

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

    // Inicializa o histórico caso seja a primeira execução.
    if (manager.getHistory().empty())
        manager.createHistory(role, thinking);

    manager.compressIfNeeded(role);
    manager.addHistory("user", prompt);

    const Ollama::json toolSchemas = tools.getSchema();
    const Ollama::json noTools = Ollama::json::array();

    // Reserva a última rodada para o modelo produzir a
    // resposta final, sem depender de novas ferramentas.
    const int maxRounds = std::max(
        1,
        static_cast<int>(Config::Properties::max_rounds));

    int rounds = 0;

    ChatResult response;

    // =======================================================
    // Centraliza uma chamada completa ao modelo.
    // Toda chamada passa pelos mesmos callbacks e histórico.
    // =======================================================

    auto requestModel = [&](bool toolsAllowed) -> ChatResult
    {
        manager.compressIfNeeded(role);

        manager.alterSystemHistory(
            role,
            thinking,
            toolsAllowed);

        ChatResult result = ollama.chatStream(
            model,
            manager.getHistory(),

            // Texto gerado pelo modelo.
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
            toolsAllowed ? toolSchemas : noTools);

        ++rounds;

        if (audio)
            speech.flush();

        // Guarda a resposta completa, incluindo tool calls
        // e pensamento, antes de executar ferramentas.
        manager.addHistory(
            "assistant",
            result.response,
            result.toolCalls,
            "",
            result.think);

        return result;
    };

    // =======================================================
    // Observa e registra os resultados das ferramentas.
    // =======================================================

    auto observeTool = [&](const std::string &name,
                           const ToolResult &result)
    {
        const std::string toolOutput =
            result.success
                ? result.output
                : "Falha ao executar a ferramenta: " + result.error;

        // Formato de resposta de ferramenta esperado pelo Ollama.
        manager.addHistory(
            "tool",
            toolOutput,
            Ollama::json::array(),
            name);

        if (onTool)
            onTool(name, result);
    };

    // =======================================================
    // Loop principal do agente
    // =======================================================

    while (rounds < maxRounds)
    {
        // A última rodada fica reservada para resposta final.
        const bool toolsAllowed = rounds < maxRounds - 1;

        response = requestModel(toolsAllowed);

        // O modelo terminou sem solicitar ferramentas.
        if (response.toolCalls.empty())
            return response.response;

        // Não executa chamadas de ferramentas na rodada final.
        if (!toolsAllowed)
            return response.response;

        // Executa todas as ferramentas solicitadas.
        taskManager.toolCalling(
            response.toolCalls,
            observeTool);

        // Se nenhuma ferramenta iniciou um plano,
        // o loop principal continua normalmente.
        if (!taskManager.hasActivePlan())
            continue;

        // ===================================================
        // Execução do plano
        // ===================================================

        while (taskManager.hasActivePlan() &&
               rounds < maxRounds - 1)
        {
            // Instrui o Agent sobre a etapa atual.
            manager.addHistory(
                "system",
                taskManager.currentInstruction());

            // Permite várias chamadas de ferramentas
            // durante a execução de uma mesma etapa.
            while (taskManager.hasActivePlan() &&
                   rounds < maxRounds - 1)
            {
                response = requestModel(true);

                if (!response.toolCalls.empty())
                {
                    taskManager.toolCalling(
                        response.toolCalls,
                        observeTool);

                    continue;
                }

                const bool stepDone =
                    response.response.find("[STEP_DONE]") !=
                    std::string::npos;

                const bool stepFailed =
                    response.response.find("[STEP_FAILED]") !=
                    std::string::npos;

                // Somente STEP_DONE, sem STEP_FAILED,
                // confirma a conclusão da etapa.
                taskManager.reportStep(
                    stepDone && !stepFailed);

                // Sai do loop interno para instruir a próxima etapa.
                break;
            }
        }

        // Se ainda existir um plano ativo, o limite global
        // impediu sua conclusão.
        if (taskManager.hasActivePlan())
        {
            taskManager.blockActivePlan(
                "Limite global de rodadas do agente atingido.");
        }

        // Registra o resultado completo do planejamento.
        const std::string report = taskManager.finalReport();

        manager.addHistory("system", report);

        // A última rodada é usada para transformar o relatório
        // em uma resposta natural para o usuário.
        if (rounds < maxRounds)
        {
            response = requestModel(false);
            return response.response;
        }

        return report;
    }

    return response.response;
}

// ===========================================================
// Controle de áudio
// ===========================================================

void Orquestrador::setAudioEnabled(bool enabled)
{
    if (!enabled)
    {
        speech.cancelPending();
        speech.wait();
    }
}

// ===========================================================
// Callback de fala
// ===========================================================

void Orquestrador::setSpeechCallback(Speach::Speaker callback)
{
    speech.setPhraseCallback(std::move(callback));
}