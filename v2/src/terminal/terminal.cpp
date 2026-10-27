
#include "terminal.h"

#include <algorithm>
#include <iostream>
#include <cmath>
#include <numbers>
#include <ftxui/dom/canvas.hpp>

using namespace ftxui;

// ============================================================
// MENSAGENS
// ============================================================

Component TerminalDisplay::initDisplay()
{
    return Renderer([]
                    { return hbox({text("Bem-vindo!") | color(Color::Red),
                                   text(" Envie: ") | color(Color::White),
                                   text("/help") | color(Color::Magenta),
                                   text(" - Para exibir todos os comandos") | color(Color::White)}); });
}

Component TerminalDisplay::thinkingStatus()
{
    return Renderer([this]
                    {
        if (!isGenerating.load())
            return text("");

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - generationStart)
                .count();

        const auto minutes = elapsed / 60;
        const auto seconds = elapsed % 60;

        std::string time =
            (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
            (seconds < 10 ? "0" : "") + std::to_string(seconds);

        const int frame = spinnerFrame.load();

        return hbox({
            spinner(2, frame) | bold | color(Color::Yellow2),
            text(" Pensando... ") | color(Color::Yellow2),
            text(time) | color(Color::White),
        }); });
}

void TerminalDisplay::startThinking()
{
    generationStart = std::chrono::steady_clock::now();
    isGenerating.store(true);

    timerThread = std::jthread([this](std::stop_token stop)
                               {
    while (!stop.stop_requested() && isGenerating.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!stop.stop_requested() && isGenerating.load())
        {
            ++spinnerFrame;
            screen.PostEvent(Event::Custom);
        }
    } });

    screen.PostEvent(Event::Custom);
}

double TerminalDisplay::stopThinking()
{
    isGenerating.store(false);

    if (timerThread.joinable())
    {
        timerThread.request_stop();
        timerThread.join();
    }

    screen.PostEvent(Event::Custom);

    return std::chrono::duration<double>(
               std::chrono::steady_clock::now() - generationStart)
        .count();
}

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

        if (role != "Assistant")
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
                        {"/models", "Exibe os modelos atuais"},
                        {"/audio", "Ativa ou desativa o modo de audio"}});

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

void TerminalDisplay::setStatus(const std::string &status)
{
    {
        std::lock_guard<std::mutex> lock(chat_mutex);
        statusMessage = status;
    }

    screen.PostEvent(Event::Custom);
}

void TerminalDisplay::setAudioStatus(const bool status)
{
    audioEnabled.store(status);
    // ====== LIMPEZA DA FILA VISUAL AO SAIR DO MODO VOZ ======
    if (!status)
    {
        std::lock_guard lock(chat_mutex);
        speechCaptions.clear();
        lastCaption.clear();
    }

    screen.PostEvent(Event::Custom);
}

// ====== FILA VISUAL COM DURAÇÃO ESTIMADA, SEM BLOQUEAR O TTS ======
void TerminalDisplay::showSpeechPhrase(const std::string &phrase)
{
    if (phrase.find_first_not_of(" \t\r\n") == std::string::npos)
        return;
    {
        std::lock_guard lock(chat_mutex);

        if (!audioEnabled.load())
            return;
        if (speechCaptions.empty())
            captionStart = std::chrono::steady_clock::now();
        speechCaptions.push_back(phrase);
    }
    screen.PostEvent(Event::Custom);
}

// ============================================================
// CANVAS DE VOZ: CIRCUNFERÊNCIA RGB E LEGENDA DA RESPOSTA
// ============================================================
Element TerminalDisplay::voiceView()
{
    const auto now = std::chrono::steady_clock::now();
    const double time = std::chrono::duration<double>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();

    const bool generating = isGenerating.load();

    bool speaking = false;
    std::string caption;
    {
        std::lock_guard lock(chat_mutex);

        // ====== AVANÇO ESTIMADO DA LEGENDA NA ORDEM DO TTS ======
        while (!speechCaptions.empty())
        {
            const auto &phrase = speechCaptions.front();
            const auto characters = std::count_if(phrase.begin(), phrase.end(),
                                                  [](unsigned char c)
                                                  { return (c & 0xC0) != 0x80; });

            const auto duration = std::chrono::duration<double>(
                std::max(0.8, static_cast<double>(characters) / 14.0));

            lastCaption = phrase;

            if (now - captionStart < duration)
                break;

            captionStart += std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration);
            speechCaptions.pop_front();
        }

        speaking = !speechCaptions.empty();
        caption = lastCaption;
    }

    if (caption.empty())
        caption = generating ? "Preparando a resposta..." : "Digite uma mensagem para conversar.";

    // ====== GEOMETRIA PARAMÉTRICA E DEFORMAÇÃO POR SENO E COSSENO ======
    const int columns = std::max(1, voiceBox.x_max - voiceBox.x_min + 1);
    const int rows = std::max(1, voiceBox.y_max - voiceBox.y_min + 1);

    Canvas drawing(columns * 2, rows * 4);

    const double radius = std::max(0.5, std::min(columns * 0.78, rows * 1.45));
    const double amplitude = speaking ? 0.11 : 0.018;

    for (int point = 0; point < 180; ++point)
    {
        const double angle = point * 2.0 * std::numbers::pi / 180.0;
        const double wave = 0.65 * std::sin(6.0 * angle + time * 3.5) +
                            0.35 * std::cos(9.0 * angle - time * 2.4);
        const double r =
            radius * (1.0 + amplitude * wave + 0.02 * std::sin(time * 1.6));

        const int x =
            static_cast<int>(columns + r * std::cos(angle));
        const int y =
            static_cast<int>(rows * 2 + r * std::sin(angle));

        const int red = static_cast<int>(45 + 40 * std::sin(angle + time));
        const int green = static_cast<int>(195 + 50 * std::sin(angle + time * 0.7));
        const int blue = static_cast<int>(205 + 50 * std::cos(angle - time * 0.5));

        drawing.DrawPoint(x, y, true, Color::RGB(red, green, blue));
        drawing.DrawPoint(x + 1, y, true, Color::RGB(red / 3, green / 3, blue / 3));
    }

    auto orb = canvas(std::move(drawing)) | flex | reflect(voiceBox);
    auto subtitle = paragraph(caption) | color(Color::RGB(160, 220, 245)) |
                    size(HEIGHT, LESS_THAN, 4) | yframe;

    return vbox({orb, separator(),
                 text(speaking ? "AGENTE DE VOZ · Fala estimada" : (generating ? "AGENTE DE VOZ · Preparando" : "AGENTE DE VOZ · Pronto")) |
                     bold | color(Color::Cyan),
                 subtitle}) |
           flex;
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
    | yframe
    | flex; });

    // --------------------------------------------------------
    // Entrada
    // --------------------------------------------------------

    auto input = displayInput(onSubmit);

    // --------------------------------------------------------
    // Barra de status
    // --------------------------------------------------------

    auto statusBar = Renderer([this]
                              {
    std::string status;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);
        status = statusMessage;
    }

    return hbox({
        text(" ◌ " + status)
            | color(Color::Yellow),

        filler(),

        text("Modo de fala: ") | color(Color::Yellow2),
        text(audioEnabled.load() ? "ativado" : "desativado")
            | color(Color::Cyan),
        text(" | "),

        text("[Enter] Enviar")
            | color(Color::Green),

        text(" | [Ctrl + C] Sair")
            | color(Color::Red)
    })
    | bgcolor(Color::Palette256(236)); });

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
        // ====== O MODO VOZ PRESERVA AS TECLAS DE EDIÇÃO DA ENTRADA ======
        if (audioEnabled.load())
            return false;
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

    auto init = initDisplay();
    auto ts = thinkingStatus();

    // --------------------------------------------------------
    // Renderização final
    // --------------------------------------------------------

    auto renderer = Renderer(events, [&]
                             {
    Elements elements;

    // ====== ALTERNÂNCIA ENTRE CONVERSA E PERSPECTIVA DE VOZ ======
    if (audioEnabled.load())
    {
        elements.push_back(voiceView());
        if (showCommands)
            elements.push_back(commandView());
        elements.push_back(input->Render());
        elements.push_back(ts->Render());
        elements.push_back(statusBar->Render());
        return vbox(std::move(elements)) | borderRounded |
               color(Color::RGB(150, 210, 245)) | bgcolor(Color::Black);
    }

    // Conversa ocupa o espaço disponível.
    elements.push_back(
        init -> Render() | flex
    );

    elements.push_back(
        chatScroll->Render() | flex
    );

    // Comandos opcionais.
    if (showCommands)
        elements.push_back(commandView());

    // Entrada fixa.
    elements.push_back(
        input->Render()
    );

    elements.push_back(
        ts->Render()
    );

    // Barra de status fixa.
    elements.push_back(
        statusBar->Render()
    );

    return vbox(std::move(elements)) | flex; });

    screen.TrackMouse(true);
    // ====== RELÓGIO VISUAL INDEPENDENTE DA INFERÊNCIA ======
    input->TakeFocus();
    voiceAnimation = std::jthread([this](std::stop_token stop)
                                  {
        while (!stop.stop_requested())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            if (!stop.stop_requested() && audioEnabled.load())
                screen.PostEvent(Event::Custom);
        } });
    screen.Loop(renderer);
    voiceAnimation.request_stop();
    voiceAnimation.join();
}
