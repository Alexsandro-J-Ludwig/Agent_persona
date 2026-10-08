#ifndef TERMINAL_H
#define TERMINAL_H

#include <ftxui/ftxui.hpp>
#include <string>

using namespace ftxui;
using MessageId = std::uint64_t;

struct ChatMessage
{
    MessageId id;
    std::string role;
    std::string message;
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

private:
    std::string prompt;
    std::vector<ChatMessage> messages;
    std::mutex chat_mutex;
    MessageId nextId = 0;

    ScreenInteractive screen =
        ScreenInteractive::TerminalOutput();

    Component displayInput(SubmitCallback onSubmit);
};

#endif