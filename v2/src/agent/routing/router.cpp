#include "router.h"
#include <algorithm>
#include <cctype>
#include <iostream>

// =========================================================
// Forca o pensmento a partir de mensagens contidas no texto
// =========================================================
void Router::applyHeuristic(const std::string &prompt)
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

// =============================================================
// Decide a rota de pensamento a partir do usuario
// =============================================================
std::pair<bool, std::string> Router::routeDecide(const std::string &prompt)
{
    std::string texto = prompt;

    if (prompt.find("/fast") != std::string::npos)
    {
        agent.model = ModelRole::Agent;
        agent.thinking = false;

        texto.erase(0, 5);

        return {true, texto};
    }
    else if (prompt.find("/think") != std::string::npos)
    {
        agent.model = ModelRole::Think;
        agent.thinking = true;

        texto.erase(0, 6);

        return {true, texto};
    }

    return {false, texto};
}

void Router::decideThink(const std::string &prompt)
{
    ToolResult result = tool.executeSystemTool(
        "decide_think",
        prompt);

    if (!result.success)
        return; // Preserva a decisão heurística.

    try
    {
        json decision = json::parse(result.output);

        std::string modelName =
            decision["model"].get<std::string>();

        if (modelName != "Agent" && modelName != "Think")
            return;

        agent.model =
            modelName == "Think"
                ? ModelRole::Think
                : ModelRole::Agent;

        agent.thinking =
            decision["thinking"].get<bool>();
    }
    catch (const json::exception &)
    {
        // Mantém a decisão heurística.
    }
}
// ==============================================================
// Rota de orquestracao
// ==============================================================
std::tuple<ModelRole, bool> Router::route(
    const std::string &prompt)
{
    auto [select, texto] = routeDecide(prompt);

    // Comando explícito tem prioridade.
    if (select)
        return {agent.model, agent.thinking};

    // Classificação heurística inicial.
    applyHeuristic(texto);

    // Classificação semântica quando a heurística
    // não selecionou o modelo Think.
    if (agent.model == ModelRole::Agent)
    {
        decideThink(texto);
    }

    return {agent.model, agent.thinking};
}
