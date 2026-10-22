#ifndef TOOL_SCHEMAS_H
#define TOOL_SCHEMAS_H

#include <json.hpp>

namespace ToolSchemas
{
    inline nlohmann::json calculator()
    {
        return {
            {"type", "function"},
            {"function", {
                {"name", "calculator"},
                {"description", "Executa uma operação matemática."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"a", {{"type", "number"}}},
                        {"b", {{"type", "number"}}},
                        {"operation", {
                            {"type", "string"},
                            {"enum", {"+", "-", "*", "/"}}
                        }}
                    }},
                    {"required", {"a", "b", "operation"}}
                }}
            }}
        };
    }
}

#endif