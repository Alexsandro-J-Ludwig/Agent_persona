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
            orquestrador.run(
                prompt,
                [this, id](const std::string& chunk)
                {
                    terminal.appendChunk(id, chunk);
                }
            );
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