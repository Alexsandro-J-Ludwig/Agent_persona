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
        return json::object({{"type", "function"},
                             {"function", json::object({{"name", name},
                                                        {"description", description},
                                                        {"parameters", json::object({{"type", "object"},
                                                                                     {"properties", std::move(properties)},
                                                                                     {"required", std::move(required)}})}})}});
    }

    inline json prop(const std::string &type, const std::string &description)
    {
        return json::object({{"type", type},
                             {"description", description}});
    }

    inline json calculator()
    {
        json operation = prop("string", "Arithmetic operator.");
        operation["enum"] = json::array({"+", "-", "*", "/"});

        return makeTool(
            "calculator",
            "Performs one arithmetic operation on two numbers. "
            "Args: a, b (numbers), operation (+, -, *, /).",
            json::object({{"a", prop("number", "First operand.")},
                          {"b", prop("number", "Second operand.")},
                          {"operation", operation}}),
            json::array({"a", "b", "operation"}));
    }

    inline json searchFiles()
    {
        return makeTool(
            "searchFiles",
            "Searches accessible file and folder names across the system, case-insensitively. "
            "Windows: available drives, up to 100 results; directory reparse points are not followed. "
            "Linux: starts at /, skips /proc, /sys, /dev and /run, up to 10 results. "
            "Args: termo (string, min 3 characters).",
            json::object({{"termo", prop("string", "Part of the file name to look for (min 3 characters).")}}),
            json::array({"termo"}));
    }

    inline json listDirectory()
    {
        return makeTool(
            "listDirectory",
            "Lists the entries of a directory without descending into subfolders "
            "(directories end with '/'). Args: path (string, optional, defaults to the current directory).",
            json::object({{"path", prop("string", "Directory to list, relative to the current directory or absolute inside the workspace.")}}));
    }

    inline const json &all()
    {
        static const json schemas = json::array({calculator(),
                                                 searchFiles(),
                                                 listDirectory()});
        return schemas;
    }
}

#endif
