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
    };

    inline nlohmann::json searchFiles()
{
    return {
        {"type", "function"},      // <- estava vazio
        {"function", {
            {"name", "searchFiles"},
            {"description", "Busca arquivos pelo nome na pasta do usuario."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"termo", {{"type", "string"},
                               {"description", "Parte do nome do arquivo (minimo 3 caracteres)"}}}
                }},
                {"required", nlohmann::json::array({"termo"})}
            }}
        }}
    };
}
}

#endif