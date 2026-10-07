#include "historyManager.h"

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