#ifndef AGENTORCHESTRATOR_H
#define AGENTORCHESTRATOR_H

#include <ollama.h>
#include "../configs/models.h"
#include "router.h"
#include "historyManager.h"

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