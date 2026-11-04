#ifndef CHAT_H
#define CHAT_H

#include "src/agent/orchestrator/agentOrchestrator.h"
#include "src/terminal/terminal.h"

class Chat
{
private:
    TerminalDisplay terminal;
    Orquestrador orquestrador;

    std::thread ai_thread;
    std::atomic<bool> processing{false};
    std::optional<MessageId> current;
    std::optional<MessageId> thinkCurrent;
    std::optional<MessageId> toolsCurrent;

    size_t toolCount = 0;

    void sendMessage(const std::string &prompt);

public:
    void run();
};

#endif