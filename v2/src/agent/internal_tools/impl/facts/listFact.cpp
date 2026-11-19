#include "internalTools.h"

ToolResult InternalTools::listFacts(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const int32_t limit = args.value("limit", 20);

        if (limit < 1 || limit > 100)
            return {false, "", "O limite deve estar entre 1 e 100."};

        const auto result = Memory::instance().listFact(limit);

        return {result.success, result.output, result.error};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}