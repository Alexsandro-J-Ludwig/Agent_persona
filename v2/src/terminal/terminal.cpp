
#include "terminal.h"

#include <algorithm>
#include <iostream>

// ============================================================
// MENSAGENS
// ============================================================

MessageId TerminalDisplay::addMessage(
    const std::string &role,
    const std::string &message)
{
    MessageId id;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        id = ++nextId;

        ChatMessage entry{id, role, message};

        entry.markdown.append(message);

        if (role == "User")
            entry.markdown.finish();

        messages.push_back(std::move(entry));
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
                entry.markdown.append(chunk);
                updated = true;
                break;
            }
        }
    }

    if (updated)
        screen.PostEvent(Event::Custom);
}

// ============================================================
// ENTRADA DO USUÁRIO
// ============================================================

Component TerminalDisplay::displayInput(
    SubmitCallback onSubmit)
{
    auto input = Input(&prompt, "Pergunte à IA...");

    auto renderer = Renderer(input, [input]
                             { return hbox({text("User: ") | color(Color::Blue),
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

            if (submitted == "/help")
            {
                showCommands = !showCommands;
                screen.PostEvent(Event::Custom);
                return true;
            }

            // Retorna ao final ao enviar nova mensagem.
            scrollPosition = 1.0f;

            onSubmit(submitted);
            return true;
        }

        return false; });
}

// ============================================================
// TABELA DE COMANDOS
// ============================================================

Element TerminalDisplay::commandView()
{
    auto table = Table({{"Commands", "Function"},
                        {"/fast", "Force response speed"},
                        {"/think", "Force thinking mode"},
                        {"/models", "Exibe os modelos atuais"}});

    table.SelectAll().Separator(LIGHT);

    table.SelectRow(0).Decorate(bold);
    table.SelectRow(0).Decorate(color(Color::Cyan));

    table.SelectColumn(0).Decorate(color(Color::BlueLight));
    table.SelectColumn(1).Decorate(color(Color::GrayLight));

    return vbox({text("  COMANDOS DISPONÍVEIS") |
                     bold |
                     color(Color::Cyan),

                 separator(),

                 table.Render()}) |
           borderRounded | bgcolor(Color::Palette256(235)) | size(WIDTH, GREATER_THAN, 48);
}

// ============================================================
// INTERFACE PRINCIPAL
// ============================================================

void TerminalDisplay::run(SubmitCallback onSubmit)
{
    // --------------------------------------------------------
    // Renderização do histórico
    // --------------------------------------------------------

    auto chatRenderer = Renderer([this]
                                 {
    Elements entries;

    std::lock_guard<std::mutex> lock(chat_mutex);

    for (const auto& entry : messages)
    {
        entries.push_back(
            vbox({
                hbox({
                    text(entry.role + ": ")
                        | bold
                        | color(entry.role == "User"
                            ? Color::Blue
                            : Color::Red),

                    terminal_markdown::render(
                        entry.markdown.document()
                    ) | flex
                }),

                separator()
            })
        );
    }

    return vbox(std::move(entries))
    | focusPositionRelative(0.0f, scrollPosition)
    | vscroll_indicator
    | frame
    | flex; });
    // --------------------------------------------------------
    // Entrada
    // --------------------------------------------------------

    auto input = displayInput(onSubmit);

    // --------------------------------------------------------
    // Barra de status
    // --------------------------------------------------------

    auto statusBar = Renderer([]
                              { return hbox({text(" [Enter] Enviar ") | color(Color::Green),
                                             text(" | [Ctrl + C] Sair") | color(Color::Red),
                                             filler()}) |
                                       bgcolor(Color::Palette256(236)); });

    // --------------------------------------------------------
    // Eventos de rolagem
    // --------------------------------------------------------

    auto chatScroll = CatchEvent(chatRenderer, [this](Event event)
                                 {
        // Teclado
        if (event == Event::PageUp)
        {
            scrollPosition = std::max(
                0.0f, scrollPosition - 0.25f);
            return true;
        }

        if (event == Event::PageDown)
        {
            scrollPosition = std::min(
                1.0f, scrollPosition + 0.25f);
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

        // Mouse
        if (event.is_mouse())
        {
            if (event.mouse().button == Mouse::WheelUp)
            {
                scrollPosition = std::max(
                    0.0f, scrollPosition - 0.05f);
                return true;
            }

            if (event.mouse().button == Mouse::WheelDown)
            {
                scrollPosition = std::min(
                    1.0f, scrollPosition + 0.05f);
                return true;
            }
        }

        return false; });

    // --------------------------------------------------------
    // Layout e eventos globais
    // --------------------------------------------------------

    auto layout = Container::Vertical({chatScroll,
                                       input});

    // Permite rolar mesmo quando o input está focado.
    auto events = CatchEvent(layout, [this](Event event)
                             {
        if (event == Event::PageUp)
        {
            scrollPosition = std::max(
                0.0f, scrollPosition - 0.25f);
            return true;
        }

        if (event == Event::PageDown)
        {
            scrollPosition = std::min(
                1.0f, scrollPosition + 0.25f);
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
                scrollPosition = std::max(
                    0.0f, scrollPosition - 0.05f);
                return true;
            }

            if (event.mouse().button == Mouse::WheelDown)
            {
                scrollPosition = std::min(
                    1.0f, scrollPosition + 0.05f);
                return true;
            }
        }

        return false; });

    // --------------------------------------------------------
    // Renderização final
    // --------------------------------------------------------

    auto renderer = Renderer(events, [&]
                             {
    Elements elements;

    elements.push_back(chatScroll->Render() | flex);

    if (showCommands)
        elements.push_back(commandView());

    elements.push_back(input->Render());
    elements.push_back(statusBar->Render());

    return vbox(std::move(elements)) | flex; });

    screen.TrackMouse(true);
    screen.Loop(renderer);
}
