#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include "src/ollama/ollama.h"
#include "src/configs/agentconfig.h"
#include <vector>
#include <string>

class InternalTools;

class HistoryManager
{
public:
    explicit HistoryManager(InternalTools &tools) : tools(tools) {}

    void createHistory(ModelRole role, bool thinking);
    void alterSystemHistory(ModelRole role, bool thinking);

    void addHistory(const std::string &role, const std::string &message);

    void compressor();

    const std::vector<Ollama::json> &getHistory() const;

private:
    InternalTools &tools;
    std::vector<Ollama::json> history;
    const Config::Properties properties;
};

#endif
