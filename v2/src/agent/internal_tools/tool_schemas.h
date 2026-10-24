#ifndef TOOL_SCHEMAS_H
#define TOOL_SCHEMAS_H

#include <json.hpp>
#include <string>
#include <utility>

namespace ToolSchemas
{
    using json = nlohmann::json;

    inline json makeTool(const std::string &name,
                         const std::string &description,
                         json properties = json::object(),
                         json required = json::array())
    {
        return json::object({
            {"type", "function"},
            {"function", json::object({
                {"name", name},
                {"description", description},
                {"parameters", json::object({
                    {"type", "object"},
                    {"properties", std::move(properties)},
                    {"required", std::move(required)}
                })}
            })}
        });
    }

    inline json prop(const std::string &type, const std::string &description)
    {
        return json::object({
            {"type", type},
            {"description", description}
        });
    }

    inline json calculator()
    {
        json operation = prop("string", "Arithmetic operator.");
        operation["enum"] = json::array({"+", "-", "*", "/"});

        return makeTool(
            "calculator",
            "Performs one arithmetic operation on two numbers. "
            "Args: a, b (numbers), operation (+, -, *, /).",
            json::object({
                {"a", prop("number", "First operand.")},
                {"b", prop("number", "Second operand.")},
                {"operation", operation}
            }),
            json::array({"a", "b", "operation"}));
    }

    inline json searchFiles()
    {
        return makeTool(
            "searchFiles",
            "Searches file and folder names inside the workspace "
            "(case-insensitive, up to 5 levels deep, max 10 results). "
            "Args: termo (string, min 3 characters).",
            json::object({
                {"termo", prop("string", "Part of the file name to look for (min 3 characters).")}
            }),
            json::array({"termo"}));
    }

    inline json listDirectory()
    {
        return makeTool(
            "listDirectory",
            "Lists the entries of a directory without descending into subfolders "
            "(directories end with '/'). Args: path (string, optional, defaults to the current directory).",
            json::object({
                {"path", prop("string", "Directory to list, relative to the current directory or absolute inside the workspace.")}
            }));
    }

    // inline json listTool()
    // {
    //     return makeTool(
    //         "listTool",
    //         "Returns the parameter schema of a tool by its name. "
    //         "Args: tool_name (string).",
    //         json::object({
    //             {"tool_name", prop("string", "Name of the tool to inspect.")}
    //         }),
    //         json::array({"tool_name"}));
    // }

    inline json enterDirectory()
    {
        return makeTool(
            "enterDirectory",
            "Changes the agent's current directory. Cannot leave the workspace. "
            "Args: path (string, relative to the current directory or absolute inside the workspace).",
            json::object({
                {"path", prop("string", "Directory to enter.")}
            }),
            json::array({"path"}));
    }

    inline json exitDirectory()
    {
        return makeTool(
            "exitDirectory",
            "Moves the agent's current directory to its parent. "
            "Stops at the workspace root. Takes no arguments.");
    }

    inline const json &all()
    {
        static const json schemas = json::array({
            calculator(),
            searchFiles(),
            listDirectory(),
            // listTool(),
            enterDirectory(),
            exitDirectory()
        });
        return schemas;
    }
}

#endif