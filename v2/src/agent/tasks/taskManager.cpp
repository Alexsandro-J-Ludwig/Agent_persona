
#include "taskManager.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

// ============================================================
// AUXILIARES
// ============================================================

namespace
{
    std::string trim(const std::string &value)
    {
        const auto start = value.find_first_not_of(" \t\r\n");

        if (start == std::string::npos)
            return "";

        const auto end = value.find_last_not_of(" \t\r\n");

        return value.substr(start, end - start + 1);
    }

    std::string toLower(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        return value;
    }
}

// ============================================================
// EXECUÇÃO DAS FERRAMENTAS
// ============================================================

void TaskManager::toolCalling(
    const nlohmann::json &toolCalls,
    const OnToolResult &onResult)
{
    std::vector<PendingTool> pending;

    pending.reserve(toolCalls.size());

    // Dispara as ferramentas.
    for (const auto &call : toolCalls)
    {
        std::string name = "desconhecida";

        try
        {
            const auto &fn = call.at("function");

            name = fn.at("name").get<std::string>();

            std::string args = "{}";

            if (fn.contains("arguments"))
            {
                args = fn.at("arguments").is_string()
                           ? fn.at("arguments").get<std::string>()
                           : fn.at("arguments").dump();
            }

            pending.push_back({name,
                               tools.execute(
                                   ToolCaller::Model,
                                   name,
                                   args)});
        }
        catch (const std::exception &e)
        {
            if (onResult)
                onResult(name, {false, "", e.what()});
        }
    }

    // Aguarda os resultados.
    for (auto &task : pending)
    {
        ToolResult result;

        try
        {
            result = task.future.get();
        }
        catch (const std::exception &e)
        {
            result = {false, "", e.what()};
        }

        // Uma ferramenta pode iniciar um plano.
        if (result.success && result.kind == ToolKind::Plan)
        {
            try
            {
                startPlanManagement(result.output);
            }
            catch (const std::exception &e)
            {
                result.success = false;
                result.error = e.what();
            }
        }

        if (onResult)
            onResult(task.name, result);
    }
}

// ============================================================
// INICIALIZAÇÃO DO PLANO
// ============================================================

void TaskManager::startPlanManagement(const std::string &planText)
{
    if (hasActivePlan())
        throw std::logic_error(
            "Ja existe um plano em execucao.");

    ActivePlan plan;

    plan.text = planText;
    plan.steps = parseSteps(planText);
    plan.maxAttempts = parseMaxAttempts(planText);

    plan.currentStep = 0;
    plan.attempts = 0;

    plan.status = plan.steps.empty()
                      ? TaskStatus::Blocked
                      : TaskStatus::InProgress;

    if (plan.steps.empty())
        plan.blockedReason = "O plano nao possui etapas validas.";

    activePlan = std::move(plan);
}

void TaskManager::blockActivePlan(const std::string &reason)
{
    if (!activePlan)
        throw std::logic_error(
            "Nao existe plano para bloquear.");

    if (activePlan->status != TaskStatus::InProgress)
        return;

    activePlan->status = TaskStatus::Blocked;
    activePlan->blockedReason = reason;
}

// ============================================================
// EXTRAÇÃO DAS ETAPAS
// ============================================================

std::vector<std::string> TaskManager::parseSteps(
    const std::string &planText) const
{
    std::vector<std::string> steps;

    std::istringstream stream(planText);
    std::string line;

    bool insideSteps = false;

    const std::regex stepPattern(
        R"(^\s*(?:\d+[.)]|[-*])\s+(.+?)\s*$)");

    while (std::getline(stream, line))
    {
        line = trim(line);

        if (line.empty())
            continue;

        const std::string lower = toLower(line);

        // Identifica o início da lista de etapas.
        if (!insideSteps)
        {
            if (lower.find("steps:") != std::string::npos)
                insideSteps = true;

            continue;
        }

        // Finaliza a leitura da lista.
        if (lower.find("maximum attempts:") != std::string::npos)
            break;

        std::smatch match;

        if (std::regex_match(line, match, stepPattern))
        {
            std::string step = trim(match[1].str());

            if (!step.empty())
                steps.push_back(std::move(step));
        }
        else if (line.back() == ':')
        {
            // Encontrou outra seção.
            break;
        }
    }

    return steps;
}

// ============================================================
// EXTRAÇÃO DO LIMITE DE TENTATIVAS
// ============================================================

int TaskManager::parseMaxAttempts(
    const std::string &planText) const
{
    const std::regex pattern(
        R"(Maximum\s+attempts\s*:\s*(\d+))",
        std::regex_constants::icase);

    std::smatch match;

    if (!std::regex_search(planText, match, pattern))
        return 3;

    try
    {
        const int attempts = std::stoi(match[1].str());

        return std::max(1, attempts);
    }
    catch (...)
    {
        return 3;
    }
}

// ============================================================
// INSTRUÇÃO DA ETAPA ATUAL
// ============================================================

std::string TaskManager::currentInstruction() const
{
    if (!activePlan)
        throw std::logic_error("Nenhum plano ativo.");

    const auto &plan = *activePlan;

    if (plan.status != TaskStatus::InProgress)
        throw std::logic_error("O plano nao esta em execucao.");

    if (plan.currentStep >= plan.steps.size())
        throw std::logic_error("Indice da etapa invalido.");

    std::ostringstream instruction;

    instruction
        << "PLANO ATIVO.\n"
        << "Etapa " << (plan.currentStep + 1)
        << "/" << plan.steps.size() << ": "
        << plan.steps[plan.currentStep] << "\n\n"

        << "Tentativa: " << (plan.attempts + 1)
        << "/" << plan.maxAttempts << "\n\n"

        << "Execute SOMENTE esta etapa.\n"
        << "Ao concluir, responda com [STEP_DONE].\n"
        << "Se nao conseguir concluir, responda com [STEP_FAILED].";

    return instruction.str();
}

// ============================================================
// RESULTADO DA ETAPA
// ============================================================

void TaskManager::reportStep(bool done)
{
    if (!activePlan)
        throw std::logic_error("Nao existe plano para atualizar.");

    auto &plan = *activePlan;

    if (plan.status != TaskStatus::InProgress)
        throw std::logic_error("O plano nao esta em execucao.");

    if (plan.currentStep >= plan.steps.size())
        throw std::logic_error("Indice da etapa invalido.");

    if (plan.attempts >= plan.maxAttempts)
    {
        plan.status = TaskStatus::Blocked;
        plan.blockedReason =
            "O numero maximo de tentativas foi atingido.";
    }

    if (done)
    {
        // Etapa concluída.
        plan.attempts = 0;
        ++plan.currentStep;

        // Verifica se todas as etapas foram concluídas.
        if (plan.currentStep >= plan.steps.size())
            plan.status = TaskStatus::Finished;
    }
    else
    {
        // Etapa não concluída.
        ++plan.attempts;

        if (plan.attempts >= plan.maxAttempts)
            plan.status = TaskStatus::Blocked;
    }
}

// ============================================================
// RELATÓRIO FINAL
// ============================================================

std::string TaskManager::finalReport() const
{
    if (!activePlan)
        return "Nenhum plano foi executado.";

    const auto &plan = *activePlan;

    std::ostringstream report;

    report << "RELATORIO DE EXECUCAO DO PLANO\n\n";

    switch (plan.status)
    {
    case TaskStatus::Finished:
        report << "STATUS: FINISHED\n";
        report << "Todas as etapas foram concluidas.\n";
        break;

    case TaskStatus::Blocked:
        report << "STATUS: BLOCKED\n";
        report << "O plano nao conseguiu concluir todas as etapas.\n";
        break;

    case TaskStatus::InProgress:
        report << "STATUS: IN PROGRESS\n";
        break;

    case TaskStatus::Pending:
        report << "STATUS: PENDING\n";
        break;
    }

    report << "\nEtapas concluidas: "
           << plan.currentStep
           << "/" << plan.steps.size() << "\n";

    if (plan.status == TaskStatus::Blocked &&
        plan.currentStep < plan.steps.size())
    {
        report << "\nEtapa bloqueada: "
               << plan.steps[plan.currentStep] << "\n";

        report << "Tentativas realizadas: "
               << plan.attempts << "/"
               << plan.maxAttempts << "\n";
    }

    if (!plan.blockedReason.empty())
    {
        report << "\nMotivo do bloqueio: "
               << plan.blockedReason << "\n";
    }

    return report.str();
}
