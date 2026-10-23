#include "taskManager.hpp"

#include <future>
#include <vector>
#include <string>
#include <utility>

void TaskManager::toolCalling(const nlohmann::json &toolCalls,
                              const OnToolResult &onResult)
{
    std::vector<PendingTool> pending;
    pending.reserve(toolCalls.size());

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

            pending.push_back({name, tools.execute(ToolCaller::Model, name, args)});
        }
        catch (const std::exception &e)
        {
            if (onResult)
                onResult(name, {false, "", e.what()});
        }
    }

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

        if (onResult)
            onResult(task.name, result);
    }
}
