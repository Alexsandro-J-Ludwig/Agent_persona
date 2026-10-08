#include "terminal.h"
#include <iostream>

MessageId TerminalDisplay::addMessage(
    const std::string &role,
    const std::string &message)
{
    MessageId id;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        id = ++nextId;
        messages.push_back({id, role, message});
    }

    screen.PostEvent(Event::Custom);

    return id;
}

void TerminalDisplay::appendChunk(
    MessageId id,
    const std::string &chunk)
{
    bool updated = false;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        for (auto &entry : messages)
        {
            if (entry.id == id)
            {
                entry.message += chunk;
                updated = true;
                break;
            }
        }
    }

    if (updated)
        screen.PostEvent(Event::Custom);
}

Component TerminalDisplay::displayInput(
    SubmitCallback onSubmit)
{
    auto input = Input(&prompt, "Pergunte à IA...");

    auto renderer = Renderer(input, [input]
                             { return hbox({filler(),
                                            text("User: ") | color(Color::Blue),
                                            input->Render() | flex}) |
                                      border; });

    return CatchEvent(renderer, [this, onSubmit](Event event)
                      {
        if (event == Event::Return)
        {
            if (prompt.empty())
                return true;

            std::string submitted = prompt;
            prompt.clear();

            onSubmit(submitted);

            return true;
        }

        return false; });
}

void TerminalDisplay::run(SubmitCallback onSubmit)
{
    auto chatRenderer = Renderer([this]
                                 {
        Elements entries;

        std::lock_guard<std::mutex> lock(chat_mutex);

        for (const auto& entry : messages)
        {
            entries.push_back(
                vbox({
                    hbox({
                        text(entry.role + ": ") | bold |
                            color(entry.role == "User"
                                ? Color::Blue
                                : Color::Red),

                        paragraph(entry.message) | flex
                    }),
                    separator()
                })
            );
        }

        return vbox(std::move(entries))
            | vscroll_indicator
            | frame
            | flex; });

    auto input = displayInput(onSubmit);

    auto layout = Container::Vertical({chatRenderer,
                                       input});

    auto renderer = Renderer(layout, [&]
                             { return vbox({chatRenderer->Render() | flex,
                                            input->Render()}) |
                                      flex; });

    screen.Loop(renderer);
}