#include "internalTools.h"
#include "tool_schemas.h"

namespace
{
    constexpr const char *ARG_ERROR_PREFIX = "Invalid arguments";

    // Identifica erros de parse/argumento sem depender de texto solto.
    bool isArgumentError(const std::string &error)
    {
        return error.find("json.exception") != std::string::npos ||
               error.rfind(ARG_ERROR_PREFIX, 0) == 0;
    }

    std::future<ToolResult> readyResult(ToolResult result)
    {
        return std::async(std::launch::deferred,
                          [r = std::move(result)]() mutable
                          { return r; });
    }
}

InternalTools::json InternalTools::getSchema() const
{
    return ToolSchemas::all();
}

std::future<ToolResult> InternalTools::execute(
    ToolCaller caller,
    std::string_view name,
    const std::string &arguments)
{
    for (const auto &tool : TOOLS)
    {
        if (name != tool.name)
            continue;

        if (tool.caller != caller)
            return readyResult({false, "", "Ferramenta não autorizada"});

        return std::async(
            std::launch::async,
            [this, handler = tool.handler,
             args = arguments, toolName = std::string(tool.name)]() -> ToolResult
            {
                ToolResult result = (this->*handler)(args);

                if (!result.success && isArgumentError(result.error))
                {
                    result.error = "Invalid arguments for '" + toolName +
                                   "'. Expected parameters: " + parametersOf(toolName) +
                                   ". Details: " + result.error;
                }

                return result;
            });
    }

    return readyResult({false, "", "Ferramenta não encontrada"});
}

std::string InternalTools::parametersOf(const std::string &toolName) const
{
    for (const auto &schema : ToolSchemas::all())
    {
        const auto &fn = schema.at("function");

        if (fn.at("name") != toolName)
            continue;

        std::string out;
        for (const auto &[param, _] : fn.at("parameters").at("properties").items())
        {
            if (!out.empty())
                out += ", ";
            out += param;
        }
        return out;
    }

    return "";
}