#include "chat.h"

#include <atomic>
#include <exception>
#include <optional>
#include <string>
#include <thread>

void Chat::sendMessage(const std::string &prompt)
{
    if (processing.exchange(true))
        return;

    if (ai_thread.joinable())
        ai_thread.join();

    // Mensagem do usuário.
    terminal.addMessage("User", prompt);

    ai_thread = std::thread([this, prompt]()
                            {
        struct ProcessingGuard
        {
            std::atomic_bool& flag;

            ~ProcessingGuard()
            {
                flag.store(false);
            }
        } guard{processing};

        bool timerStarted = false;

        std::optional<MessageId> current;
        std::optional<MessageId> thinkCurrent;

        // Finaliza as mensagens de resposta e pensamento abertas,
        // além do agrupador de ferramentas, caso exista.
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

        try
        {
            auto [role, thinking, audio, texto] =
                orquestrador.selectModel(prompt);

            terminal.setAudioStatus(audio);

            if (texto.find_first_not_of(" \t\n\r") == std::string::npos)
            {
                terminal.setStatus("");
                return;
            }

            terminal.setStatus(
                std::string(
                    role == ModelRole::Think ? "Think" : "Agent"
                ) +
                (thinking ? " | Thinking ON" : " | Thinking OFF")
            );

            terminal.startThinking();
            timerStarted = true;

            orquestrador.run(
                texto,
                role,
                thinking,
                audio,

                // =========================================
                // RESPOSTA DO MODELO
                // =========================================
                [&](const std::string& chunk)
                {
                    if (!current)
                    {
                        if (chunk.find_first_not_of(" \t\r\n") ==
                            std::string::npos)
                        {
                            return;
                        }

                        // Finaliza o pensamento anterior.
                        if (thinkCurrent)
                        {
                            terminal.finishMessage(*thinkCurrent);
                            thinkCurrent.reset();
                        }

                        current = terminal.addMessage(
                            "IA",
                            "",
                            true
                        );
                    }

                    terminal.appendChunk(*current, chunk);
                },

                // =========================================
                // PENSAMENTO DO MODELO
                // =========================================
                [&](const std::string& chunk)
                {
                    if (!thinkCurrent)
                    {
                        if (chunk.find_first_not_of(" \t\r\n") ==
                            std::string::npos)
                        {
                            return;
                        }

                        // Finaliza uma resposta anterior, se existir.
                        if (current)
                        {
                            terminal.finishMessage(*current);
                            current.reset();
                        }

                        thinkCurrent = terminal.addMessage(
                            "Thinking",
                            "",
                            true
                        );
                    }

                    terminal.appendChunk(*thinkCurrent, chunk);
                },

                // =========================================
                // RESULTADO DE UMA FERRAMENTA
                // =========================================
                [&](const std::string& name, const ToolResult& result)
                {
                    // A chamada de ferramenta encerra os canais
                    // de resposta/pensamento do round anterior.
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

                    // O TerminalDisplay cuida de todo o agrupamento.
                    terminal.appendToolResult(
                        name,
                        result.success,
                        result.output,
                        result.error
                    );
                }
            );

            // Finaliza os componentes da execução.
            finishOpenMessages();

            terminal.stopThinking();
            timerStarted = false;

            terminal.setStatus("");
        }
        catch (const std::exception& e)
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
                    "\n\n**Erro na geração:** " + std::string(e.what())
                );
            }

            finishOpenMessages();

            if (!hasResponse)
                terminal.addMessage("Erro", e.what());

            terminal.setStatus("Erro na geração");
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
                    "\n\n**Erro desconhecido na geração.**"
                );
            }

            finishOpenMessages();

            if (!hasResponse)
            {
                terminal.addMessage(
                    "Erro",
                    "Ocorreu um erro desconhecido."
                );
            }

            terminal.setStatus("Erro na geração");
        } });
}

void Chat::run()
{
    // Conecta a notificação de fala à legenda.
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