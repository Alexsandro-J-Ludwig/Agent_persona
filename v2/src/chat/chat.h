#ifndef CHAT_H
#define CHAT_H

#include "src/agent/orchestrator/agentOrchestrator.h"
#include "src/agent/memory/memory.h"
#include "src/terminal/terminal.h"

#include <optional>
#include <string>
#include <thread>
#include <cstdint>

class Chat
{
private:
    TerminalDisplay terminal;
    Orquestrador orquestrador;
    Memory &memory = Memory::instance();

    int32_t chatID = 0;

    std::thread ai_thread;

    void sendMessage(const std::string &prompt);

public:
    void run();
};

#endif