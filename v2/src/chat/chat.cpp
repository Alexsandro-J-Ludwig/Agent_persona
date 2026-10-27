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
    std::optional<MessageId> current; 

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
        orquestrador.run(texto, role, thinking, audio,
            // texto do modelo: vai para a mensagem atual
            [&](const std::string &chunk)
            {
                if (!current)
                {
                    if (chunk.find_first_not_of(" \t\r\n") == std::string::npos)
                        return;                               
                    current = terminal.addMessage("IA", "");
                }
                terminal.appendChunk(*current, chunk);
            },

            // tool: mostra e fecha a mensagem atual; a próxima rodada abre outra
            [&](const std::string &name, const ToolResult &r)
            {
                terminal.addMessage("Tool", name + " → " + (r.success ? r.output : "erro: " + r.error));
                current.reset();
            });

        terminal.stopThinking();
        timerStarted = false;

        terminal.setStatus("");
    }
    catch (const std::exception &e)
    {
        if (timerStarted)
            terminal.stopThinking();

        if (current)
            terminal.appendChunk(*current, "\n\n[Erro: " + std::string(e.what()) + "]");
        else
            terminal.addMessage("Erro", e.what());

        terminal.setStatus("Erro na geração");
    }

    processing = false; });
}

void Chat::run()
{
    // ====== CHAT CONECTA A NOTIFICAÇÃO DE FALA À LEGENDA ======
    orquestrador.setSpeechCallback([this](const std::string &phrase)
                                 { terminal.showSpeechPhrase(phrase); });
    terminal.run([this](const std::string &prompt)
                 { sendMessage(prompt); });

    if (ai_thread.joinable())
        ai_thread.join();
}
