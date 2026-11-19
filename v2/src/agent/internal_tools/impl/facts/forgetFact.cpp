#include "internalTools.h"

ToolResult InternalTools::forgetFact(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const auto id = args.at("id").get<int32_t>();

        if (id <= 0)
            return {false, "", "ID inválido."};

        const auto result = Memory::instance().forgetFact(id);

        return {result.success, result.output, result.error};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}