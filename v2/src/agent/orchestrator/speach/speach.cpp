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
    // ====== LIMPA O MARKDOWN ANTES DE TESTAR E ENFILEIRAR A FRASE ======
    pendingSpeech += audioCleaner(token);

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

    if (hasEndPunct || hasLongClause)
    {
        if (pendingSpeech.find_first_not_of(" \t\r\n") != std::string::npos)
        {
            enqueue(pendingSpeech);
            pendingSpeech.clear();
        }
    }
}

void Speach::flush()
{
    // ====== FINALIZA A RESPOSTA SEM VAZAR ESTADO PARA A PRÓXIMA ======
    pendingSpeech += audioCleaner("", true);
    std::string text = std::move(pendingSpeech);
    pendingSpeech.clear();
    markdownBuffer.clear();
    insideCodeBlock = false;
    atLineStart = true;
    fenceLength = 3;
    fenceMarker = '`';
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
std::string Speach::audioCleaner(const std::string &text, bool final)
{
    markdownBuffer += text;
    std::string result;
    std::size_t i = 0;

    // ====== PREFIXOS DE LINHA E CERCAS PODEM CHEGAR EM CHUNKS DIFERENTES ======
    while (i < markdownBuffer.size())
    {
        if (atLineStart || insideCodeBlock)
        {
            const auto newline = markdownBuffer.find('\n', i);
            const auto end = newline == std::string::npos ? markdownBuffer.size() : newline;
            std::size_t first = i;
            while (first < end && (markdownBuffer[first] == ' ' || markdownBuffer[first] == '\t'))
                ++first;

            if (insideCodeBlock)
            {
                if (newline == std::string::npos && !final)
                    break;

                std::size_t fenceEnd = first;
                while (fenceEnd < end && markdownBuffer[fenceEnd] == fenceMarker)
                    ++fenceEnd;

                if (first - i <= 3 && fenceEnd - first >= fenceLength &&
                    markdownBuffer.find_first_not_of(" \t\r", fenceEnd) >= end)
                    insideCodeBlock = false;

                i = end + (newline != std::string::npos ? 1 : 0);
                continue;
            }

            if (first == end)
            {
                if (newline == std::string::npos && !final)
                    break;
                result += '\n';
                i = end + (newline != std::string::npos ? 1 : 0);
                continue;
            }

            const char marker = markdownBuffer[first];
            std::size_t after = first;
            while (after < end && markdownBuffer[after] == marker)
                ++after;

            if ((marker == '`' || marker == '~') && first - i <= 3)
            {
                if (after == end && newline == std::string::npos && !final)
                    break;

                if (after - first >= 3)
                {
                    if (newline == std::string::npos && !final)
                        break;
                    insideCodeBlock = true;
                    fenceMarker = marker;
                    fenceLength = after - first;
                    i = end + (newline != std::string::npos ? 1 : 0);
                    continue;
                }
            }

            // ====== TÍTULOS E LISTAS SOMENTE NO INÍCIO DA LINHA ======
            std::size_t prefixEnd = first;
            if (marker == '#')
            {
                if (after == end && newline == std::string::npos && !final)
                    break;
                if (after - first <= 6 && after < end &&
                    (markdownBuffer[after] == ' ' || markdownBuffer[after] == '\t'))
                    prefixEnd = after + 1;
            }
            else if (marker == '*' || marker == '-' || marker == '+')
            {
                if (first + 1 == end && newline == std::string::npos && !final)
                    break;
                if (first + 1 < end &&
                    (markdownBuffer[first + 1] == ' ' || markdownBuffer[first + 1] == '\t'))
                    prefixEnd = first + 2;
            }
            else if (marker >= '0' && marker <= '9')
            {
                std::size_t numberEnd = first;
                while (numberEnd < end && markdownBuffer[numberEnd] >= '0' && markdownBuffer[numberEnd] <= '9')
                    ++numberEnd;
                if (newline == std::string::npos && !final &&
                    (numberEnd == end ||
                     (numberEnd + 1 == end && (markdownBuffer[numberEnd] == '.' || markdownBuffer[numberEnd] == ')'))))
                    break;
                if (numberEnd + 1 < end &&
                    (markdownBuffer[numberEnd] == '.' || markdownBuffer[numberEnd] == ')') &&
                    (markdownBuffer[numberEnd + 1] == ' ' || markdownBuffer[numberEnd + 1] == '\t'))
                    prefixEnd = numberEnd + 2;
            }

            i = prefixEnd;
            atLineStart = false;
            continue;
        }

        // ====== MARCADORES INLINE: GUARDA O ÚLTIMO CARACTERE AMBÍGUO ======
        const char c = markdownBuffer[i];
        if (c == '*' || c == '_' || c == '~')
        {
            if (i + 1 == markdownBuffer.size() && !final)
                break;
            if (i + 1 < markdownBuffer.size() && markdownBuffer[i + 1] == c)
            {
                i += 2;
                continue;
            }
        }

        if (c != '`')
            result += c;
        if (c == '\n')
            atLineStart = true;
        ++i;
    }

    markdownBuffer.erase(0, i);
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
