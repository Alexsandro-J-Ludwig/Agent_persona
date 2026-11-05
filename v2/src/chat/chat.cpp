#include "chat.h"

#include <exception>
#include <optional>
#include <string>
#include <thread>

void Chat::sendMessage(const std::string &prompt)
{
    // false = já existe uma execução; a mensagem entrou na fila.
    if (!orquestrador.submit(prompt))
    {
        orquestrador.interruptSpeech(); // novo
        terminal.addMessage("User", prompt);
        return;
    }

    if (ai_thread.joinable())
        ai_thread.join();

    terminal.addMessage("User", prompt);

    ai_thread = std::thread([this, prompt]()
                            {
        bool timerStarted = false;

        std::optional<MessageId> current;
        std::optional<MessageId> thinkCurrent;

        auto finishOpenMessages = [&]()
        {
            if (current)
            {
                terminal.finishMessage(*current);
                current.reset();
            }
            if (thinkCurrent)
            {
                terminal.finishMessage(*thinkCurrent);
                thinkCurrent.reset();
            }
            terminal.finishToolGroup();
        };

        auto onChunk = [&](const std::string &chunk)
        {
            if (!current)
            {
                if (chunk.find_first_not_of(" \t\r\n") == std::string::npos)
                    return;

                if (thinkCurrent)
                {
                    terminal.finishMessage(*thinkCurrent);
                    thinkCurrent.reset();
                }
                current = terminal.addMessage("IA", "", true);
            }
            terminal.appendChunk(*current, chunk);
        };

        auto onThink = [&](const std::string &chunk)
        {
            if (!thinkCurrent)
            {
                if (chunk.find_first_not_of(" \t\r\n") == std::string::npos)
                    return;

                if (current)
                {
                    terminal.finishMessage(*current);
                    current.reset();
                }
                thinkCurrent = terminal.addMessage("Thinking", "", true);
            }
            terminal.appendChunk(*thinkCurrent, chunk);
        };

        auto onTool = [&](const std::string &name, const ToolResult &result)
        {
            if (current)
            {
                terminal.finishMessage(*current);
                current.reset();
            }
            if (thinkCurrent)
            {
                terminal.finishMessage(*thinkCurrent);
                thinkCurrent.reset();
            }
            terminal.appendToolResult(name, result.success, result.output, result.error);
        };

        try
        {
            auto [role, thinking, audio, texto] = orquestrador.selectModel(prompt);

            terminal.setAudioStatus(audio);

            if (texto.find_first_not_of(" \t\n\r") == std::string::npos)
            {
                terminal.setStatus("");
                orquestrador.release();
                return;
            }

            terminal.setStatus(
                std::string(role == ModelRole::Think ? "Think" : "Agent") +
                (thinking ? " | Thinking ON" : " | Thinking OFF"));

            terminal.startThinking();
            timerStarted = true;

            std::string next = texto;
            do
            {
                orquestrador.run(next, role, thinking, audio, onChunk, onThink, onTool);
                finishOpenMessages();

                // Mensagens enviadas durante a execução entram via drainPending().
                next.clear();
            } while (orquestrador.hasPendingOrRelease());

            terminal.stopThinking();
            timerStarted = false;
            terminal.setStatus("");
        }
        catch (const std::exception &e)
        {
            if (timerStarted)
            {
                terminal.stopThinking();
                timerStarted = false;
            }

            const bool hasResponse = current.has_value();

            if (hasResponse)
            {
                terminal.appendChunk(
                    *current,
                    "\n\n**Erro na geração:** " + std::string(e.what()));
            }

            finishOpenMessages();

            if (!hasResponse)
                terminal.addMessage("Erro", e.what());

            terminal.setStatus("Erro na geração");
            orquestrador.release();
        }
        catch (...)
        {
            if (timerStarted)
            {
                terminal.stopThinking();
                timerStarted = false;
            }

            const bool hasResponse = current.has_value();

            if (hasResponse)
            {
                terminal.appendChunk(
                    *current,
                    "\n\n**Erro desconhecido na geração.**");
            }

            finishOpenMessages();

            if (!hasResponse)
                terminal.addMessage("Erro", "Ocorreu um erro desconhecido.");

            terminal.setStatus("Erro na geração");
            orquestrador.release();
        } });
}

void Chat::run()
{
    orquestrador.setSpeechCallback(
        [this](const std::string &phrase)
        {
            terminal.showSpeechPhrase(phrase);
        });

    terminal.run(
        [this](const std::string &prompt)
        {
            sendMessage(prompt);
        });

    if (ai_thread.joinable())
        ai_thread.join();
}