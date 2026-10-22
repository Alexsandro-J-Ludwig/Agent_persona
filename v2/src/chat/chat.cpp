#include "chat.h"

#include <string>

void Chat::sendMessage(const std::string &prompt)
{
    if (processing.exchange(true))
        return;

    if (ai_thread.joinable())
        ai_thread.join();

    // Guarda a mensagem do usuário no historico de conversa
    terminal.addMessage("User", prompt);

    // Cria uma thread para não travar o sistema e permitir que seja iniciado um processo de contagem de
    // de tempo para debbug
    ai_thread = std::thread([this, prompt]()
                            {
    bool timerStarted = false;
    std::optional<std::size_t> id;

    try
    {
        auto [role, thinking, audio, texto] =
            orquestrador.selectModel(prompt);

        terminal.setAudioStatus(audio);

        if (texto.find_first_not_of(" \t\n\r") == std::string::npos)
        {
            terminal.setStatus("");
            processing = false;
            return;
        }

        id = terminal.addMessage("IA", "");


        // Feedback visual para o usuario identificar o que o modelo está fazendo <- MERECE REVISÃO
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
            texto,
            role,
            thinking,
            audio,
            [this, messageId = *id](const std::string& chunk)
            {
                terminal.appendChunk(messageId, chunk);
            }
        );

        const double elapsed = terminal.stopThinking();
        timerStarted = false;

        terminal.setStatus("");
    }
    catch (const std::exception &e)
    {
        if (timerStarted)
            terminal.stopThinking();

        if (id)
            terminal.appendChunk(*id, "\n\n[Erro: " + std::string(e.what()) + "]");
        else
            terminal.addMessage("Erro", e.what());

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