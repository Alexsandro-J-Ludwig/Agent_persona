#include "historyManager.h"
#include "../internal_tools/internalTools.h"

void HistoryManager::createHistory(ModelRole role, bool thinking)
{
    const std::string systemPrompt = properties.getSystemPrompt(role, thinking);

    history.clear();

    history.push_back({{"role", "system"},
                       {"content", systemPrompt}});
}

void HistoryManager::alterSystemHistory(ModelRole role, bool thinking)
{
    const std::string systemPrompt = properties.getSystemPrompt(role, thinking);

    history[0]["content"] = systemPrompt;
}

void HistoryManager::addHistory(
    const std::string &role,
    const std::string &message)
{
    if (properties.max_msg == 0)
        return;

    if (history.size() >= properties.max_msg)
    {
        compressor();
    }

    history.push_back({{"role", role},
                       {"content", message}});

    std::cerr << "\n========== LOG HISTORICO ==========\n"
              << "[History] Role: " << role << '\n'
              << "[History] Content: " << message << '\n'
              << "===================================\n"
              << std::flush;
}
// ===========================================================
// Comprensao de contexto
// ===========================================================
void HistoryManager::compressor()
{
    std::vector<Ollama::json> messages(
        history.begin() + 1,
        history.end());

    auto summary = tools.executeSystemTool(
        "compressor",
        Ollama::json(messages).dump());

    if (!summary.success)
        throw std::runtime_error("Falha ao comprimir histórico: " + summary.error);

    if (summary.output.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("O compressor retornou um resumo vazio.");

    history.erase(
        history.begin() + 1,
        history.end());

    history.push_back({{"role", "system"},
                       {"content", summary.output}});

    std::cerr << "\n========== LOG HISTORICO RESUMO ==========\n"
              << "[History] Role: system\n"
              << "[History] Content: " << summary.output << '\n'
              << "============================================\n"
              << std::flush;
}

const std::vector<Ollama::json> &
HistoryManager::getHistory() const
{
    return history;
}
