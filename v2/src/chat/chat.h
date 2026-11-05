#ifndef CHAT_H
#define CHAT_H

#include "src/agent/orchestrator/agentOrchestrator.h"
#include "src/terminal/terminal.h"

#include <optional>
#include <string>
#include <thread>

class Chat
{
private:
    TerminalDisplay terminal;
    Orquestrador orquestrador;
    std::thread ai_thread;

    void sendMessage(const std::string &prompt);

public:
    void run();
};

#endif