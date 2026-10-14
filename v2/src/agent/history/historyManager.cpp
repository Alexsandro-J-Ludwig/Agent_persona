#include "historyManager.h"

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
    history.push_back({{"role", role},
                       {"content", message}});
}

const std::vector<Ollama::json> &
HistoryManager::getHistory() const
{
    return history;
}