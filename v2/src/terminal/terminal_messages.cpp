#include "terminal.h"

#include <algorithm>
#include <mutex>
#include <utility>

using namespace ftxui;

MessageId TerminalDisplay::addMessage(
    const std::string &role,
    const std::string &message,
    bool streaming,
    std::string label)
{
    MessageId id;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        id = ++nextId;

        ChatMessage entry;
        entry.id = id;
        entry.role = role;
        entry.message = message;
        entry.label = std::move(label);
        entry.markdown.append(message);

        if (!streaming)
        {
            entry.markdown.finish();
            entry.finished = true;
        }

        messages.push_back(std::move(entry));
        pendingUiMessages.push_back(id);
    }

    screen.PostEvent(Event::Custom);
    return id;
}

void TerminalDisplay::appendChunk(MessageId id, const std::string &chunk)
{
    if (chunk.empty())
        return;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        const auto it = std::find_if(
            messages.begin(),
            messages.end(),
            [id](const ChatMessage &entry)
            {
                return entry.id == id;
            });

        if (it == messages.end() || it->finished)
            return;

        it->message += chunk;
        it->markdown.append(chunk);
        awaitToken = false;
    }

    screen.PostEvent(Event::Custom);
}

void TerminalDisplay::finishMessage(MessageId id)
{
    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        const auto it = std::find_if(
            messages.begin(),
            messages.end(),
            [id](const ChatMessage &entry)
            {
                return entry.id == id;
            });

        if (it == messages.end() || it->finished)
            return;

        it->markdown.finish();
        it->finished = true;
    }

    screen.PostEvent(Event::Custom);
}
