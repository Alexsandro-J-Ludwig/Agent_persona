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
        try
        {
            auto [role, thinking] = orquestrador.selectModel(prompt);

            terminal.setStatus(
                std::string(role == ModelRole::Think ? "Think" : "Agent") +
                (thinking ? " | Thinking ON" : " | Thinking OFF")
            );

            orquestrador.run(
                prompt,
                role,
                thinking,
                [this, id](const std::string& chunk)
                {
                    terminal.appendChunk(id, chunk);
                }
            );

            terminal.setStatus("");
        }
        catch (const std::exception& e)
        {
            terminal.appendChunk(
                id,
                std::string("\n[Erro: ") + e.what() + "]"
            );
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