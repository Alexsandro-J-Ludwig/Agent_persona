#include "chat.h"

#include <iostream>
#include <string>

void Chat::run()
{
    while (true)
    {
        std::string prompt;

        std::cout << "User: ";
        std::getline(std::cin, prompt);

        if (!std::cin)
            break;

        if (prompt.empty())
            continue;

        std::cout << "IA: ";

        orquestrador.run(
            prompt,
            [](const std::string &chunk)
            {
                std::cout << chunk << std::flush;
            });

        std::cout << '\n';
    }
}