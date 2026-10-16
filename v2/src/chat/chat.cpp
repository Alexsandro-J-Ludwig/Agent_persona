#include "chat.h"

#include <string>

void Chat::sendMessage(const std::string &prompt)
{
    if (processing.exchange(true))
        return;

    if (ai_thread.joinable())
        ai_thread.join();

    terminal.addMessage("User", prompt);
    auto id = terminal.addMessage("IA", "");

    ai_thread = std::thread([this, prompt, id]()
                            {
    bool timerStarted = false;

    try
    {
        // Faz o orquestrador selecionar o modelo e retonar
        auto [role, thinking, audio] =
            orquestrador.selectModel(prompt);

        // Envia uma mensagem de status para o usuario identificar 
        // Se o modelo esta pensando ou nao
        terminal.setStatus(
            std::string(
                role == ModelRole::Think ? "Think" : "Agent"
            ) +
            (thinking ? " | Thinking ON" : " | Thinking OFF")
        );

        // Inicia o cronometro de contagem de tempo de resposta do agente
        terminal.startThinking();
        timerStarted = true;

        // Realiza o chamado principal do orquestrador
        orquestrador.run(
            prompt,
            role,
            thinking,
            audio,
            [this, id](const std::string& chunk)
            {
                terminal.appendChunk(id, chunk);
            }
        );

        const double elapsed = terminal.stopThinking();
        timerStarted = false;

        terminal.setStatus("");
    }
    catch (const std::exception& e)
    {
        if (timerStarted)
        {
            const double elapsed = terminal.stopThinking();
            terminal.appendChunk(
                id,
                "\n\n[Erro: " + std::string(e.what()) + "]"
            );
        }
        else
        {
            terminal.appendChunk(
                id,
                "\n\n[Erro: " + std::string(e.what()) + "]"
            );
        }

        terminal.setStatus("Erro na geração");
    }

    processing = false; });
}

void Chat::run()
{
    terminal.run([this](const std::string &prompt)
                 { sendMessage(prompt); });

    if (ai_thread.joinable())
        ai_thread.join();
}