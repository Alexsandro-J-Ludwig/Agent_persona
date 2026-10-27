#include "internalTools.h"

ToolResult InternalTools::readCode(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);

        const fs::path filePath = args.at("filePath").get<std::string>();

        const int init = args.value("init", 0);
        const int ends = args.value("ends", 0);

        const int start = (init > 0) ? init : 1;
        const int end = (ends > 0) ? ends : std::numeric_limits<int>::max();

        if (end < start)
            return {false, "", "Invalid line range"};

        if (!fs::is_regular_file(filePath))
            return {false, "", "File not found"};

        // Valida se existe
        if (!fs::exists(filePath))
            return {false, "", "File not found"};

        // Valida se o arquivo é um código
        static const std::set<std::string> allowedExt = {
            ".py", ".cpp", ".hpp", ".h", ".java", ".rs", ".c", ".cs"};

        if (!allowedExt.count(filePath.extension().string()))
            return {false, "", "Unsupported file type"};

        std::ifstream file(filePath);
        if (!file.is_open())
            return {false, "", "Could not open file"};

        std::ifstream file(filePath);

        std::string code;
        std::string line;
        int currentLine = 1;
        int totalLines = 0;
        bool truncated = false;

        // Le todo o arquivo informado no caminho
        while (std::getline(file, line))
        {
            ++totalLines;

            if (currentLine > end)
            {
                ++currentLine;
                break;
            }

            if (currentLine >= start)
            {
                if (code.size() + line.size() + 1 > MAX_BYTES)
                {
                    truncated = true;
                    break;
                }

                code += std::to_string(currentLine) + ": " + line + "\n";
            }

            ++currentLine;
        }

        std::string header = "// total_lines=" + std::to_string(totalLines) + (truncated ? ", truncated: use init para continuar" : "") + "\n";

        return {true, header + code, ""};
    }
    catch (const std::exception &e)
    {
        return {
            false,
            "",
            e.what()};
    }
}
