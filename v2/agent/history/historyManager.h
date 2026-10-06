#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <ollama.h>
#include <vector>
#include <string>

class HistoryManager
{
public:
    void addHistory(const std::string &role, const std::string &message);

    const std::vector<Ollama::json> &getHistory() const;

private:
    std::vector<Ollama::json> history;
};

#endif