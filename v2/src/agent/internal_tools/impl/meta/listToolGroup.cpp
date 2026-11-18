#include "internalTools.h"
#include "tool_schemas.h"

ToolResult InternalTools::listToolGroup(const std::string &arguments)
{
    try
    {
        const auto args = arguments.empty() ? json::object() : json::parse(arguments);
        const auto &groups = ToolSchemas::groups();

        std::lock_guard<std::mutex> lock(groupsMtx_);

        if (!args.contains("group") || args["group"].is_null())
        {
            std::string out = "Available tool groups:\n";
            for (const auto &g : groups)
            {
                out += "- " + g.name;
                out += activeGroups_.contains(g.name) ? " [enabled]" : "";
                out += ": " + g.description + "\n";
            }
            return {true, out, ""};
        }

        const std::string name = args.at("group").get<std::string>();

        const auto it = std::find_if(
            groups.begin(), groups.end(),
            [&](const auto &g)
            { return g.name == name; });

        if (it == groups.end())
            return {false, "", "Unknown group: " + name};

        activeGroups_.insert(name);

        std::string out = "Group enabled: " + name + ". Tools now available:\n";
        for (const auto &tool : it->tools)
        {
            const auto &fn = tool.at("function");
            out += "- " + fn.at("name").get<std::string>() + "\n";
        }
        return {true, out, ""};
    }
    catch (const json::exception &e)
    {
        return {false, "", std::string("Invalid arguments: ") + e.what()};
    }
}