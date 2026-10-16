#ifndef AGENTORCHESTRATOR_H
#define AGENTORCHESTRATOR_H

#include "src/ollama/ollama.h"
#include "src/configs/models.h"
#include "src/agent/routing/router.h"
#include "src/agent/history/historyManager.h"
#include "src/agent/internal_tools/internalTools.h"

class Orquestrador
{
private:
    Ollama ollama;
    Router router;
    HistoryManager manager;

public:
    Orquestrador();

    std::string run(
        const std::string &prompt,
        ModelRole role,
        bool thinking,
        bool audio,
        Ollama::StreamCallback onChunk);

    std::tuple<ModelRole, bool, bool> selectModel(
        const std::string &prompt);
};

#endif