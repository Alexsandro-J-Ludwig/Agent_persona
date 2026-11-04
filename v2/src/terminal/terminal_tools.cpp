#include "terminal.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>

using namespace ftxui;

void TerminalDisplay::appendToolResult(
    const std::string &name,
    bool success,
    const std::string &output,
    const std::string &error)
{
    MessageId id;
    std::size_t index;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        if (!toolGroupMessage)
        {
            id = ++nextId;

            ChatMessage entry;
            entry.id = id;
            entry.role = "Tools";
            entry.label = "Tools";

            messages.push_back(std::move(entry));
            pendingUiMessages.push_back(id);

            toolGroupMessage = id;
            toolCount = 0;
        }
        else
        {
            id = *toolGroupMessage;
        }

        index = ++toolCount;
    }

    std::string entry;
    entry += "\n\n";
    entry += "**(" + std::to_string(index) + ") " + name + "**";
    entry += success ? " [OK]\n\n" : " [ERRO]\n\n";

    if (!output.empty())
    {
        entry += "**Saída:**\n\n";
        entry += output;
        entry += "\n\n";
    }

    if (!success && !error.empty())
    {
        entry += "**Erro:**\n\n";
        entry += error;
        entry += "\n\n";
    }

    if (success && output.empty())
        entry += "Sem saída retornada.\n\n";

    appendChunk(id, entry);
}

void TerminalDisplay::finishToolGroup()
{
    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        if (!toolGroupMessage)
            return;

        const MessageId id = *toolGroupMessage;

        const auto it = std::find_if(
            messages.begin(),
            messages.end(),
            [id](const ChatMessage &entry)
            {
                return entry.id == id;
            });

        if (it != messages.end() && !it->finished)
        {
            it->markdown.finish();
            it->finished = true;
        }

        toolGroupMessage.reset();
        toolCount = 0;
    }

    screen.PostEvent(Event::Custom);
}
