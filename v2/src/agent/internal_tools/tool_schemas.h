#ifndef TOOL_SCHEMAS_H
#define TOOL_SCHEMAS_H

#include <json.hpp>

namespace ToolSchemas
{
    inline nlohmann::json calculator()
    {
        return {
            {"type", "function"},
            {"function", {{"name", "calculator"}, {"description", "Executa um cálculo matemático."}, {"parameters", {{"type", "object"}, {"properties", {{"expression", {{"type", "string"}}}}}, {"required", {"operation" : {
                                                                                                                                                                                                      "type" : "string",
                                                                                                                                                                                                      "enum" : [ "+", "-", "*", "/" ]
                                                                                                                                                                                                  }}}}}}}};
    }
}

#endif