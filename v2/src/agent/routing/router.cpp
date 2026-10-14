#include "router.h"
#include <algorithm>
#include <cctype>

void Router::forceThinking(const std::string &prompt)
{
    agent.model = ModelRole::Agent;
    agent.thinking = false;

    std::string texto = prompt;

    std::transform(
        texto.begin(),
        texto.end(),
        texto.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    int maiorPeso = 0;

    for (const auto &termo : busca)
    {
        if (texto.find(termo.term) != std::string::npos)
        {
            maiorPeso = std::max(maiorPeso, termo.weight);
        }
    }

    if (maiorPeso >= 6)
    {
        agent.model = ModelRole::Think;
        agent.thinking = true;
    }
    else if (maiorPeso >= 4)
    {
        agent.model = ModelRole::Think;
        agent.thinking = false;
    }
    else if (maiorPeso >= 2)
    {
        agent.model = ModelRole::Agent;
        agent.thinking = true;
    }
}

std::tuple<ModelRole, bool> Router::route(const std::string &prompt)
{
    forceThinking(prompt);

    return {agent.model, agent.thinking};
};