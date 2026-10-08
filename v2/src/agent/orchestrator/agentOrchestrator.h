#ifndef AGENTORCHESTRATOR_H
#define AGENTORCHESTRATOR_H

#include <../src/ollama/ollama.h>
#include "../src/configs/models.h"
#include "../src/agent/routing/router.h"
#include "../src/agent/history/historyManager.h"

class Orquestrador
{
private:
    Ollama ollama;
    Router router;
    HistoryManager manager;

public:
    void run(const std::string &prompt, Ollama::StreamCallback onChunk);
};

#endif