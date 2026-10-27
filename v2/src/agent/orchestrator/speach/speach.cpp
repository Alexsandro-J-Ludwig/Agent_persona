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

void Speach::feed(const std::string &token)
{
    pendingSpeech += token;

    bool hasEndPunct = (pendingSpeech.find('.') != std::string::npos ||
                        pendingSpeech.find('!') != std::string::npos ||
                        pendingSpeech.find('?') != std::string::npos ||
                        pendingSpeech.find('\n') != std::string::npos ||
                        pendingSpeech.find(", ") != std::string::npos);

    // Se a frase já passou de 45 caracteres e tem uma vírgula ou ':', já envia pro TTS!
    bool hasLongClause = (pendingSpeech.size() >= 45 &&
                          (pendingSpeech.find(',') != std::string::npos ||
                           pendingSpeech.find(':') != std::string::npos ||
                           pendingSpeech.find(';') != std::string::npos));

    if(hasEndPunct || hasLongClause)
    {
        if (pendingSpeech.find_first_not_of(" \t\r\n") != std::string::npos)
            enqueue(pendingSpeech);
        pendingSpeech.clear();
    }

    if (hasEndPunct || hasLongClause)
        flush();
}

void Speach::flush()
{
    std::string text = audioCleaner(pendingSpeech);
    pendingSpeech.clear();
    if (text.find_first_not_of(" \t\r\n") != std::string::npos)
        enqueue(std::move(text));
}

// =================================================
// Adiciona os itens a fila
// =================================================
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

// ===================================================
// Cancela a pendencia de audio
// ===================================================
void Speach::cancelPending()
{
    std::lock_guard lock(mtx);
    phrases.clear();
    if (!active)
        idle.notify_all();
}

// ===================================================
// Captura o erro
// ===================================================
std::string Speach::lastError() const
{
    std::lock_guard lock(mtx);
    return error;
}

// ====== NOTIFICAÇÃO SEM DEPENDÊNCIA DA INTERFACE ======
void Speach::setPhraseCallback(Speaker callback)
{
    std::lock_guard lock(mtx);
    phraseCallback = std::move(callback);
}

// ===================================================
// Limpa as tags markdown do texto
// ===================================================
std::string Speach::audioCleaner(const std::string &text)
{
    std::string result;
    result.reserve(text.size());

    size_t i = 0;

    while (i < text.size())
    {
        // Detecta início/fim de bloco Markdown
        if (text.compare(i, 3, "```") == 0)
        {
            insideCodeBlock = !insideCodeBlock;
            i += 3;
            continue;
        }

        // Ignora tudo dentro do bloco
        if (insideCodeBlock)
        {
            ++i;
            continue;
        }

        // Remove marcadores de formatação
        if (text.compare(i, 2, "**") == 0 ||
            text.compare(i, 2, "__") == 0 ||
            text.compare(i, 2, "~~") == 0)
        {
            i += 2;
            continue;
        }

        // Remove marcadores simples
        if (text[i] == '*' ||
            text[i] == '`' ||
            text[i] == '#')
        {
            ++i;
            continue;
        }

        result += text[i];
        ++i;
    }

    return result;
}

// ====================================================
// Consume os itens da fila
// ====================================================
void Speach::consume()
{
    while (true)
    {
        std::string phrase;
        Speaker notify;
        {
            std::unique_lock lock(mtx);
            ready.wait(lock, [this]
                       { return stopping || !phrases.empty(); });

            if (stopping && phrases.empty())
                return;

            phrase = std::move(phrases.front());

            phrases.pop_front();
            active = true;
            notify = phraseCallback;
        }

        // A chamada demorada acontece fora do mutex e fora do streaming.
        std::string failure;
        try
        {
            speaker(phrase);
            // ====== NOTIFICA A FRASE APÓS O ENVIO AO TTS ======
            if (notify)
                notify(phrase);
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
