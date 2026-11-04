#include "terminal.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include <ftxui/dom/canvas.hpp>

using namespace ftxui;

void TerminalDisplay::setAudioStatus(bool status)
{
    audioEnabled.store(status);

    if (!status)
    {
        std::lock_guard<std::mutex> lock(chat_mutex);
        speechCaptions.clear();
        captionStart = {};
    }

    screen.PostEvent(Event::Custom);
}

void TerminalDisplay::showSpeechPhrase(const std::string &phrase)
{
    if (phrase.find_first_not_of(" \t\r\n") == std::string::npos)
        return;

    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        if (!audioEnabled.load())
            return;

        if (speechCaptions.empty())
            captionStart = std::chrono::steady_clock::now();

        // Mantém a fila limitada para não acumular frases durante uma sessão longa.
        constexpr std::size_t maxQueuedCaptions = 32;
        if (speechCaptions.size() >= maxQueuedCaptions)
            speechCaptions.pop_front();

        speechCaptions.push_back(phrase);
    }

    screen.PostEvent(Event::Custom);
}

Element TerminalDisplay::voiceView()
{
    const auto now = std::chrono::steady_clock::now();
    const double time = std::chrono::duration<double>(now.time_since_epoch()).count();
    const bool generating = isGenerating.load();

    bool speaking = false;
    {
        std::lock_guard<std::mutex> lock(chat_mutex);

        // A fila é consumida com uma duração aproximada por frase. Sem um
        // callback de término do player, isso evita deixar o estado "Falando"
        // permanentemente ativo após a primeira frase.
        while (!speechCaptions.empty())
        {
            const std::size_t chars = speechCaptions.front().size();
            const auto duration = std::chrono::milliseconds(
                static_cast<long long>(std::clamp<std::size_t>(chars * 65, 700, 8000)));
            const auto phraseEnd = captionStart + duration;

            if (now < phraseEnd)
            {
                speaking = true;
                break;
            }

            captionStart = phraseEnd;
            speechCaptions.pop_front();
        }

        if (speechCaptions.empty())
            captionStart = {};
    }

    const int columns = std::max(1, voiceBox.x_max - voiceBox.x_min + 1);
    const int rows = std::max(1, voiceBox.y_max - voiceBox.y_min + 1);

    Canvas drawing(columns * 16, rows * 64);

    const double radius = std::max(0.5, std::min(columns * 0.78, rows * 1.45));
    const double amplitude = speaking ? 0.055 : 0.018;

    for (int point = 0; point < 180; ++point)
    {
        const double angle = point * 2.0 * std::numbers::pi / 180.0;
        const double wave =
            0.65 * std::sin(4.0 * angle + time * 3.5) +
            0.25 * std::cos(9.0 * angle - time * 2.4);

        const double r = radius * (1.0 + amplitude * wave + 0.012 * std::sin(time * 1.6));

        const int x = static_cast<int>(columns + r * std::cos(angle));
        const int y = static_cast<int>(rows * 2 + r * std::sin(angle));

        const int red = static_cast<int>(45 + 40 * std::sin(angle + time));
        const int green = static_cast<int>(195 + 50 * std::sin(angle + time * 0.7));
        const int blue = static_cast<int>(205 + 50 * std::cos(angle - time * 0.5));
        const int thickness = speaking ? 5 : 10;

        for (int dx = 0; dx < thickness; ++dx)
        {
            const int shade = (dx == 0) ? 6 : 5;
            drawing.DrawPoint(
                x + dx,
                y,
                true,
                Color::RGB(red / shade, green / shade, blue / shade));
        }
    }

    auto orb = canvas(std::move(drawing)) | flex | reflect(voiceBox);

    auto status = text(
                      speaking
                          ? "AGENTE DE VOZ · Falando"
                          : (generating ? "AGENTE DE VOZ · Preparando"
                                        : "AGENTE DE VOZ · Pronto")) |
                  bold | color(Color::Cyan);

    return vbox({orb, separator(), status}) | flex;
}
