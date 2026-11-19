#include "internalTools.h"

ToolResult InternalTools::rememberFact(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const auto fact = args.at("fact").get<std::string>();

        if (fact.empty())
            return {false, "", "O fato não pode estar vazio."};

        const auto result = Memory::instance().rememberFact(fact);

        return {
            result.success,
            result.output + (result.success
                                 ? "\nID do fato: " + std::to_string(result.factID)
                                 : ""),
            result.error};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}