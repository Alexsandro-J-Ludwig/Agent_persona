#ifndef TERMINAL_H
#define TERMINAL_H

#include <ftxui/ftxui.hpp>
#include <string>

#include "../src/markdown/markdownRenderer.h"

#include "../src/markdown/markdownRenderer.h"
#include "../src/markdown/markdownFtxui.h"

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
        std::function<void(const std::string &)>;

    void run(SubmitCallback onSubmit);

    MessageId addMessage(
        const std::string &role,
        const std::string &message);

    void appendChunk(const std::uint64_t id, const std::string &chunk);
    Element commandView();

private:
    std::string prompt;
    std::vector<ChatMessage> messages;
    std::mutex chat_mutex;
    MessageId nextId = 0;
    float scrollPosition = 1.0f;
    bool showCommands = false;

    MarkdownRenderer render;

    ScreenInteractive screen =
        ScreenInteractive::TerminalOutput();

    Component displayInput(SubmitCallback onSubmit);
};

#endif