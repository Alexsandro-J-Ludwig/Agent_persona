#include "internalTools.h"
#include "tool_schemas.h"

#include "src/agent/internal_tools/impl/sandbox/sandbox.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <limits>

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

bool InternalTools::isToolActive(std::string_view tool) const
{
    if (tool == "listToolGroup")
        return true;

    std::lock_guard lock(groupsMtx_);

    if (disabledTools_.count(std::string(tool)))
        return false;

    for (const auto &g : ToolSchemas::groups())
        if (activeGroups_.count(g.name))
            for (const auto &t : g.tools)
                if (t.at("function").at("name").get<std::string>() == tool)
                    return true;

    return false;
}

json InternalTools::getSchema() const
{
    json schema = ToolSchemas::initial();

    std::unordered_set<std::string> active;
    {
        std::lock_guard<std::mutex> lock(groupsMtx_);
        active = activeGroups_;
    }

    for (const auto &group : ToolSchemas::groups())
    {
        if (!active.contains(group.name))
            continue;

        for (const auto &tool : group.tools)
        {
            const std::string name = tool.at("function").at("name").get<std::string>();
            if (isToolActive(name))
                schema.push_back(tool);
        }
    }

    return schema;
}
void InternalTools::disableGroup(const std::string &group)
{
    std::lock_guard lock(groupsMtx_);
    activeGroups_.erase(group);
}

void InternalTools::setToolEnabled(const std::string &tool, bool enabled)
{
    std::lock_guard lock(groupsMtx_);
    if (enabled)
        disabledTools_.erase(tool);
    else
        disabledTools_.insert(tool);
}

// ToolResult InternalTools::listToolGroup(const std::string &arguments)
// {
//     try
//     {
//         const auto args = json::parse(arguments);

//         if (!args.contains("group"))
//         {
//             std::lock_guard lock(groupsMtx_);
//             std::string out;
//             for (const auto &g : ToolSchemas::groups())
//                 out += g.name + (activeGroups_.count(g.name) ? " [active]" : "") +
//                        ": " + g.description + "\n";
//             return {true, out};
//         }

//         const std::string group = args.at("group").get<std::string>();

//         for (const auto &g : ToolSchemas::groups())
//         {
//             if (g.name != group)
//                 continue;

//             {
//                 std::lock_guard lock(groupsMtx_);
//                 activeGroups_.insert(group);
//             }

//             std::string names;
//             for (const auto &t : g.tools)
//             {
//                 if (!names.empty())
//                     names += ", ";
//                 names += t.at("function").at("name").get<std::string>();
//             }
//             return {true, "Group enabled: " + group + ". Tools: " + names};
//         }

//         return {false, "", "Unknown group: " + group + ". Call listToolGroup without arguments to see the available groups."};
//     }
//     catch (const json::exception &e)
//     {
//         return {false, "", std::string("Invalid arguments: ") + e.what()};
//     }
// }

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

        if (caller == ToolCaller::Model && !isToolActive(name))
            return readyResult({false, "",
                                "Tool not enabled. Call listToolGroup to enable its group first."});

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