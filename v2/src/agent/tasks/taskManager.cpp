#include "taskManager.hpp"

void TaskManager::toolCalling(const nlohmann::json &toolCalls,
                              const OnToolResult &onResult)
{
    for (const auto &call : toolCalls)
    {
        std::string name = "desconhecida";
        ToolResult result{false, "", "Chamada de ferramenta inválida"};

        try
        {
            const auto &fn = call.at("function");
            name = fn.at("name").get<std::string>();

            std::string args = "{}";
            if (fn.contains("arguments"))
                args = fn.at("arguments").is_string()
                           ? fn.at("arguments").get<std::string>()
                           : fn.at("arguments").dump();

            result = tools.executeModelTool(name, args);
        }
        catch (const std::exception &e)
        {
            result = {false, "", e.what()};
        }

        if (onResult)
            onResult(name, result);
    }
}