#include <cstdio>
#include <iostream>
#include <string>

#include "ollama/ollama.h"
#include "../configs/models.h"

using namespace std;

int main()
{
    const Model &model = getModel(ModelRole::Agent);
    Ollama ollama;

    while (true)
    {
        std::string prompt;

        cout << "User: \n";
        std::getline(std::cin, prompt);

        if (!std::cin)
            break;

        if (prompt.empty())
            continue;

        auto response = ollama.generate(
            model,
            prompt,
            false);

        std::cout
            << "IA: "
            << response.value("response", "")
            << "\n\n";
    }

    return 0;
}