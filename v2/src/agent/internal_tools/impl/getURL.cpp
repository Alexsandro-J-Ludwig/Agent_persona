#include "internalTools.h"

ToolResult InternalTools::getUrls(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const std::string term = args.at("term").get<std::string>();

        json results = searx_.search(term, 5);

        if (results.empty())
            return {false, "", "Nenhuma resposta encontrada"};

        return {true, results.dump(), ""};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}