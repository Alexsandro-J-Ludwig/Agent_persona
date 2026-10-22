#ifndef AGENTORCHESTRATOR_H
#define AGENTORCHESTRATOR_H

#include "src/ollama/ollama.h"
#include "src/configs/models.h"
#include "src/agent/routing/router.h"
#include "src/agent/history/historyManager.h"
#include "src/agent/internal_tools/internalTools.h"
#include <tuple>
#include "src/agent/orchestrator/speach/speach.h"
#include "src/agent/tasks/taskManager.hpp"

using ToolCallback = std::function<void(const std::string &, const ToolResult &)>;

class Orquestrador
{
private:
    Ollama ollama;
    InternalTools tools;
    Router router;
    HistoryManager manager;
    Speach speech;
    TaskManager taskManager{tools};

public:
    Orquestrador();

    std::string run(
        const std::string &prompt,
        ModelRole role,
        bool thinking,
        bool audio,
        Ollama::StreamCallback onChunk,
        ToolCallback onTool = {}
    );

    std::tuple<ModelRole, bool, bool, std::string> selectModel(
        const std::string &prompt);

    void setAudioEnabled(bool enabled);
};

#endif
