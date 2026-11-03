#include "terminal.h"

#include <algorithm>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace ftxui;

void TerminalDisplay::run(SubmitCallback onSubmit)
{
    // O container gerencia os componentes correspondentes às mensagens.
    auto messageList = Container::Vertical({});

    auto renderMessage = [this](MessageId id, bool thinking, bool showRole) -> Element
    {
        ChatMessage msgCopy;
        {
            std::lock_guard<std::mutex> lock(chat_mutex);

            const auto it = std::find_if(
                messages.begin(),
                messages.end(),
                [id](const ChatMessage &entry)
                {
                    return entry.id == id;
                });

            if (it == messages.end())
                return text("");

            msgCopy = *it;
        }

        auto content = terminal_markdown::render(msgCopy.markdown.document());

        if (thinking)
            return vbox({content | color(Color::Grey0), separator()});

        if (!showRole)
            return vbox({content, separator()});

        auto roleElement = text(msgCopy.role + ": ");

        if (msgCopy.role == "User")
            roleElement = roleElement | color(Color::Cyan);
        else if (msgCopy.role == "IA")
            roleElement = roleElement | color(Color::Orange1);
        else
            roleElement = roleElement | color(Color::Red);

        return vbox({hbox({roleElement, content | flex}), separator()});
    };

    auto createMessageComponent = [messageList, renderMessage, this](MessageId id)
    {
        std::string role;
        std::string label;

        {
            std::lock_guard<std::mutex> lock(chat_mutex);

            const auto it = std::find_if(
                messages.begin(),
                messages.end(),
                [id](const ChatMessage &entry)
                {
                    return entry.id == id;
                });

            if (it == messages.end())
                return;

            role = it->role;
            label = it->label;
        }

        const bool isThinking = role == "Thinking";
        const bool isTools = role == "Tools";

        if (isThinking || isTools)
        {
            if (label.empty())
                label = isThinking ? "Pensamento" : "Tools";

            auto body = Renderer([renderMessage, id, isThinking]
            {
                return renderMessage(id, isThinking, false);
            });

            messageList->Add(Collapsible(label, body));
            return;
        }

        messageList->Add(Renderer([renderMessage, id]
        {
            return renderMessage(id, false, true);
        }));
    };

    auto attachPendingMessages = [this, createMessageComponent]()
    {
        std::vector<MessageId> pending;

        {
            std::lock_guard<std::mutex> lock(chat_mutex);
            pending.swap(pendingUiMessages);
        }

        for (const MessageId id : pending)
            createMessageComponent(id);
    };

    attachPendingMessages();

    auto chatRenderer = Renderer(messageList, [messageList, this]
    {
        return messageList->Render() |
               focusPositionRelative(0.0f, scrollPosition) |
               vscroll_indicator | yframe | flex;
    });

    auto input = displayInput(std::move(onSubmit));

    auto statusBar = Renderer([this]
    {
        std::string status;
        {
            std::lock_guard<std::mutex> lock(chat_mutex);
            status = statusMessage;
        }

        return hbox({
                   text(" ◌ " + status) | color(Color::Yellow),
                   filler(),
                   text("Modo de fala: ") | color(Color::Yellow2),
                   text(audioEnabled.load() ? "ativado" : "desativado") | color(Color::Cyan),
                   text(" | "),
                   text("[Enter] Enviar") | color(Color::Green),
                   text(" | [Ctrl + C] Sair") | color(Color::Red),
               }) |
               bgcolor(Color::Palette256(236));
    });

    auto chatScroll = CatchEvent(chatRenderer, [this](Event event)
    {
        if (event == Event::PageUp)
        {
            scrollPosition = std::max(0.0f, scrollPosition - 0.25f);
            return true;
        }

        if (event == Event::PageDown)
        {
            scrollPosition = std::min(1.0f, scrollPosition + 0.25f);
            return true;
        }

        if (event == Event::Home)
        {
            scrollPosition = 0.0f;
            return true;
        }

        if (event == Event::End)
        {
            scrollPosition = 1.0f;
            return true;
        }

        if (event.is_mouse())
        {
            if (event.mouse().button == Mouse::WheelUp)
            {
                scrollPosition = std::max(0.0f, scrollPosition - 0.05f);
                return true;
            }

            if (event.mouse().button == Mouse::WheelDown)
            {
                scrollPosition = std::min(1.0f, scrollPosition + 0.05f);
                return true;
            }
        }

        return false;
    });

    int voiceSplitSize = 105;
    auto voiceRenderer = Renderer([this]
    {
        return voiceView();
    });

    auto voiceLayout = ResizableSplitLeft(chatScroll, voiceRenderer, &voiceSplitSize);
    auto layout = Container::Vertical({voiceLayout, input});

    auto events = CatchEvent(layout, [this, attachPendingMessages](Event event)
    {
        if (event == Event::Custom)
            attachPendingMessages();

        if (audioEnabled.load())
            return false;

        // Navegação também funciona quando o painel de voz não está visível.
        if (event == Event::PageUp)
        {
            scrollPosition = std::max(0.0f, scrollPosition - 0.25f);
            return true;
        }

        if (event == Event::PageDown)
        {
            scrollPosition = std::min(1.0f, scrollPosition + 0.25f);
            return true;
        }

        if (event == Event::Home)
        {
            scrollPosition = 0.0f;
            return true;
        }

        if (event == Event::End)
        {
            scrollPosition = 1.0f;
            return true;
        }

        if (event.is_mouse())
        {
            if (event.mouse().button == Mouse::WheelUp)
            {
                scrollPosition = std::max(0.0f, scrollPosition - 0.05f);
                return true;
            }

            if (event.mouse().button == Mouse::WheelDown)
            {
                scrollPosition = std::min(1.0f, scrollPosition + 0.05f);
                return true;
            }
        }

        return false;
    });

    auto init = initDisplay();
    auto thinking = thinkingStatus();

    auto renderer = Renderer(events, [&]
    {
        Elements elements;

        if (audioEnabled.load())
        {
            elements.push_back(voiceLayout->Render() | flex);

            if (showCommands)
                elements.push_back(commandView());

            elements.push_back(input->Render());
            elements.push_back(thinking->Render());
            elements.push_back(statusBar->Render());

            return vbox(std::move(elements)) |
                   flex | borderRounded |
                   color(Color::RGB(150, 210, 245)) |
                   bgcolor(Color::Black);
        }

        elements.push_back(init->Render() | flex);
        elements.push_back(chatScroll->Render() | flex);

        if (showCommands)
            elements.push_back(commandView());

        elements.push_back(input->Render());
        elements.push_back(thinking->Render());
        elements.push_back(statusBar->Render());

        return vbox(std::move(elements)) | flex;
    });

    screen.TrackMouse(true);
    input->TakeFocus();

    // Atualiza o canvas de voz sem bloquear o loop principal da interface.
    if (voiceAnimation.joinable())
    {
        voiceAnimation.request_stop();
        voiceAnimation.join();
    }

    voiceAnimation = std::jthread([this](std::stop_token stop)
    {
        while (!stop.stop_requested())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));

            if (!stop.stop_requested() && audioEnabled.load())
                screen.PostEvent(Event::Custom);
        }
    });

    try
    {
        screen.Loop(renderer);
    }
    catch (...)
    {
        voiceAnimation.request_stop();
        if (voiceAnimation.joinable())
            voiceAnimation.join();
        throw;
    }

    voiceAnimation.request_stop();
    if (voiceAnimation.joinable())
        voiceAnimation.join();
}
