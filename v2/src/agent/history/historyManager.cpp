#include "historyManager.h"
#include "../internal_tools/internalTools.h"

void HistoryManager::createHistory(
    ModelRole role, bool thinking)
{
    const std::string systemPrompt = properties.getSystemPrompt(role, thinking);

    history.clear();

    history.push_back({{"role", "system"},
                       {"content", systemPrompt}});
}

void HistoryManager::alterSystemHistory(
    ModelRole role, bool thinking, bool toolsEnabled)
{
    const std::string systemPrompt =
        properties.getSystemPrompt(role, thinking, toolsEnabled);

    history[0]["content"] = systemPrompt;
}

std::size_t HistoryManager::estimateChars() const
{
    std::size_t total = 0;
    for (const auto &msg : history)
    {
        total += msg.value("content", "").size();
        if (msg.contains("tool_calls"))
            total += msg["tool_calls"].dump().size();
    }
    return total;
}

void HistoryManager::addHistory(
    const std::string &role,
    const std::string &message,
    const Ollama::json &tool_calls,
    const std::string &tool_name)
{

    Ollama::json msg = {{"role", role}, {"content", message}};
    if (!tool_calls.empty())
        msg["tool_calls"] = tool_calls;

    if (!tool_name.empty())
        msg["tool_name"] = tool_name;

    history.push_back(std::move(msg));

    std::cerr << "\n========== LOG HISTORICO ==========\n"
              << "[History] Role: " << role << '\n'
              << "[History] Content: " << message << '\n'
              << "===================================\n"
              << std::flush;
}

void HistoryManager::compressIfNeeded(ModelRole role)
{
    const std::size_t estTokens = static_cast<std::size_t>(estimateChars() / 3.5);
    const std::size_t limit = static_cast<std::size_t>(getModel(role).num_ctx * 0.7);

    if (estTokens > limit)
        compressor();
}

// ===========================================================
// Comprensao de contexto
// ===========================================================
void HistoryManager::compressor()
{
    std::vector<Ollama::json> messages(
        history.begin() + 1,
        history.end());

    auto summary = tools.execute(
        ToolCaller::System,
        "compressor",
        Ollama::json(messages).dump()).get();

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
