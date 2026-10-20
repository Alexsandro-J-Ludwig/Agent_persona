#include "speach.h"
#include <iostream>
#include <stdexcept>
#include <utility>

Speach::Speach(Speaker speaker)
    : speaker(std::move(speaker))
{
    if (!this->speaker)
        throw std::invalid_argument("Speach precisa de uma função de reprodução.");
    worker = std::thread(&Speach::consume, this);
}

Speach::~Speach()
{
    {
        std::lock_guard lock(mtx);
        stopping = true;
    }
    ready.notify_one();
    if (worker.joinable())
        worker.join();
}

void Speach::enqueue(std::string phrase)
{
    if (phrase.find_first_not_of(" \t\r\n") == std::string::npos)
        return;

    {
        std::lock_guard lock(mtx);
        if (stopping)
            throw std::logic_error("Speach está encerrando.");
        phrases.push_back(std::move(phrase));
    }
    ready.notify_one();
}

void Speach::wait()
{
    std::unique_lock lock(mtx);
    idle.wait(lock, [this]
              { return phrases.empty() && !active; });
}

void Speach::cancelPending()
{
    std::lock_guard lock(mtx);
    phrases.clear();
    if (!active)
        idle.notify_all();
}

std::string Speach::lastError() const
{
    std::lock_guard lock(mtx);
    return error;
}

void Speach::consume()
{
    while (true)
    {
        std::string phrase;
        {
            std::unique_lock lock(mtx);
            ready.wait(lock, [this]
                       { return stopping || !phrases.empty(); });
            if (stopping && phrases.empty())
                return;
            phrase = std::move(phrases.front());
            phrases.pop_front();
            active = true;
        }

        // A chamada demorada acontece fora do mutex e fora do streaming.
        std::string failure;
        try
        {
            speaker(phrase);
        }
        catch (const std::exception &exception)
        {
            failure = exception.what();
        }
        catch (...)
        {
            failure = "Erro desconhecido ao reproduzir áudio.";
        }

        {
            std::lock_guard lock(mtx);
            if (!failure.empty())
                error = failure;
            active = false;
        }
        idle.notify_all();
        if (!failure.empty())
            std::cerr << "[Speach] " << failure << '\n';
    }
}
