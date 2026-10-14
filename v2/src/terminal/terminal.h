#ifndef TERMINAL_H
#define TERMINAL_H

#include <ftxui/ftxui.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "src/markdown/markdownRenderer.h"
#include "src/markdown/markdownFtxui.h"

using namespace ftxui;

using MessageId = std::uint64_t;

struct ChatMessage
{
    MessageId id;
    std::string role;
    std::string message;
    MarkdownRenderer markdown;
};

class TerminalDisplay
{
public:
    using SubmitCallback =
        std::function<void(const std::string&)>;

    void run(SubmitCallback onSubmit);
    void setStatus(const std::string& status);

    MessageId addMessage(
        const std::string& role,
        const std::string& message
    );

    void appendChunk(
        MessageId id,
        const std::string& chunk
    );

    void startThinking();
    double stopThinking();
    
    Element commandView();

private:
    std::string prompt;
    std::vector<ChatMessage> messages;

    std::mutex chat_mutex;
    std::string statusMessage;

    std::chrono::steady_clock::time_point generationStart{};
    std::atomic<bool> isGenerating{false};
    std::jthread timerThread;

    float scrollPosition = 1.0f;
    bool showCommands = false;

    MessageId nextId = 0;

    MarkdownRenderer render;

    ScreenInteractive screen =
        ScreenInteractive::Fullscreen();

    Component displayInput(SubmitCallback onSubmit);
    Component initDisplay();
    Component thinkingStatus();
};

#endif
