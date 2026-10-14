#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include "src/ollama/ollama.h"
#include "src/configs/agentconfig.h"
#include <vector>
#include <string>

class HistoryManager
{
public:
    void createHistory(ModelRole role, bool thinking);
    void alterSystemHistory(ModelRole role, bool thinking);

    void addHistory(const std::string &role, const std::string &message);

    void compressor();

    const std::vector<Ollama::json> &getHistory() const;

private:
    std::vector<Ollama::json> history;
    const Config::Properties properties;
};

#endif