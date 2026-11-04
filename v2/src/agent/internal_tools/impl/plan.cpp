#include "internalTools.h"

static bool validatePlan(const std::string &plan)
{
    for (const char *field : {"Task:", "Description:", "Steps:", "Maximum attempts:"})
    {
        if (plan.find(field) == std::string::npos)
            return false;
    }
    return true;
}

ToolResult InternalTools::plan(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const std::string msg =
            args.at("msg").get<std::string>();

        const std::string userPrompt =
            args.at("userPrompt").get<std::string>();

        std::string prompt = std::format(
            R"(You're a agent tool was decide ir plan or not.

            ANALYS:
            Agent request: {},
            User Prompt: {}

            response only with a valid JSON:
            {{
                "plan": false
            }}

            the field "plan" is only true or false for this request
            )",
            msg, userPrompt);

        bool decidePlan = false;
        bool decided = false;

        for (int attempt = 0; attempt < 3; attempt++)
        {
            try
            {
                auto request = ollama.generate(getModel(ModelRole::Think), prompt, false);

                if (request.empty())
                    continue;

                json decide = json::parse(request.at("response").get<std::string>());

                if (!decide.is_object() || !decide.contains("plan") || !decide["plan"].is_boolean())
                    continue;

                decidePlan = decide["plan"].get<bool>();
                decided = true;
                break;
            }
            catch (const json::exception &)
            {
            }
        }

        if (!decided)
            return {false, "", "Não foi possível analisar a requisição"};

        prompt = std::format(R"(
            You are a tool for an AI agent.

You must plan and orchestrate a list of tasks aimed at fulfilling the user's request.

REQUIREMENTS:
- Fully understand the scope of the tasks, rather than focusing solely on 
  completing them as quickly as possible.
- Create a structured, easy-to-understand plan for the agent to follow.
- Plan long-term tasks that require more complex orchestration.

REQUIRED OUTPUT:
{{
Task: <name of the task>
Description: <description of the task>
Steps: <step-by-step breakdown of the task>
Maximum attempts: <Maximum number of attempts the agent should make 
                   to complete the task before marking it as blocked>
}}

The "Task" field should contain the name of the task to be performed.
The "Description" field is where you describe the task being executed.
In the "Steps" field, you will detail all the steps necessary to complete the task.
The "Maximum attempts" field is intended to prevent the agent from entering an unnecessary 
loop and to ensure the task is blocked if it cannot be completed.

You can carry out various tasks, provided that you meet the ultimate objective.

Agent prompt: {},
User prompt: {}

OUTPUT FORMAT RULES:
- The "Steps:" heading MUST be on its own line.
- Each step MUST be on a separate line.
- Each step MUST start with a sequential number followed by a period.
- "Maximum attempts:" MUST be followed by a positive integer.
- Do not place steps on the same line as the "Steps:" heading.

Example:

Task: Analyze the project
Description: Inspect the project structure and identify the required changes.
Steps:
1. Inspect the project files.
2. Identify the necessary modifications.
3. Implement and verify the changes.
Maximum attempts: 3
        )",
                             msg, userPrompt);

        if (!decidePlan)
            return {
                true,
                "Planejamento não necessário para esta requisição.",
                ""};

        for (int attempt = 0; attempt < 3; attempt++)
        {
            try
            {
                auto request = ollama.generate(getModel(ModelRole::Think), prompt, false);

                if (request.empty())
                    continue;

                const std::string planText =
                    request.at("response").get<std::string>();

                if (!validatePlan(planText))
                    continue;

                return {
                    true,
                    planText,
                    "",
                    ToolKind::Plan};
            }
            catch (const json::exception &)
            {
            }
        }

        return {
            false,
            "",
            "Não foi possível gerar o plano"};
    }
    catch (const std::exception &e)
    {
        return {
            false,
            "",
            e.what()};
    }
}