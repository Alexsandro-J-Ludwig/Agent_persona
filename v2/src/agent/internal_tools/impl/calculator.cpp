#include "internalTools.h"

ToolResult InternalTools::calculator(const std::string &arguments)
{
    try
    {
        const auto data = json::parse(arguments);

        const auto operation = data.at("operation").get<std::string>();
        const double a = data.at("a").get<double>();
        const double b = data.at("b").get<double>();

        double result;

        if (operation == "+")
            result = a + b;
        else if (operation == "-")
            result = a - b;
        else if (operation == "*")
            result = a * b;
        else if (operation == "/")
        {
            if (b == 0.0)
                return {false, "", "Divisão por zero."};

            result = a / b;
        }
        else
            return {false, "", "Operação não suportada."};

        return {true, std::to_string(result), ""};
    }
    catch (const json::exception &error)
    {
        return {false, "", error.what()};
    }
}
