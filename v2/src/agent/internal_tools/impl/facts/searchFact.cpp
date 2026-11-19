#include "internalTools.h"

ToolResult InternalTools::searchFacts(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const auto query = args.at("query").get<std::string>();

        if (query.empty())
            return {false, "", "A pesquisa não pode estar vazia."};

        const auto result = Memory::instance().searchFact(query);

        return {result.success, result.output, result.error};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}