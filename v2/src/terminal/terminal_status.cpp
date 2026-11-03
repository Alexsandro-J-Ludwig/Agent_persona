#include "terminal.h"

#include <chrono>
#include <thread>

using namespace ftxui;

Component TerminalDisplay::thinkingStatus()
{
    return Renderer([this]
    {
        if (!isGenerating.load())
            return text("");

        std::chrono::steady_clock::time_point start;
        bool waitingForToken = false;

        {
            std::lock_guard<std::mutex> lock(chat_mutex);
            start = generationStart;
            waitingForToken = awaitToken;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();

        const auto minutes = elapsed / 60;
        const auto seconds = elapsed % 60;

        const std::string time =
            (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
            (seconds < 10 ? "0" : "") + std::to_string(seconds);

        return hbox({
            spinnerElement(),
            text(waitingForToken ? " Aguardando primeiro token... " : " Pensando... ") |
                color(Color::Yellow2),
            text(time) | color(Color::White),
        });
    });
}

void TerminalDisplay::startThinking()
{
    {
        std::lock_guard<std::mutex> lock(chat_mutex);
        generationStart = std::chrono::steady_clock::now();
        awaitToken = false;
    }

    isGenerating.store(true);
    spinnerFrame.store(0);

    // Protege contra uma segunda inicialização sem encerramento da anterior.
    if (timerThread.joinable())
    {
        timerThread.request_stop();
        timerThread.join();
    }

    timerThread = std::jthread([this](std::stop_token stop)
    {
        while (!stop.stop_requested() && isGenerating.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (!stop.stop_requested() && isGenerating.load())
            {
                spinnerFrame.fetch_add(1);
                screen.PostEvent(Event::Custom);
            }
        }
    });

    screen.PostEvent(Event::Custom);
}

void TerminalDisplay::setAwaitToken()
{
    {
        std::lock_guard<std::mutex> lock(chat_mutex);
        awaitToken = true;
    }

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

    std::chrono::steady_clock::time_point start;
    {
        std::lock_guard<std::mutex> lock(chat_mutex);
        start = generationStart;
        awaitToken = false;
    }

    screen.PostEvent(Event::Custom);

    return std::chrono::duration<double>(
               std::chrono::steady_clock::now() - start)
        .count();
}

void TerminalDisplay::setStatus(const std::string &status)
{
    {
        std::lock_guard<std::mutex> lock(chat_mutex);
        statusMessage = status;
    }

    screen.PostEvent(Event::Custom);
}
