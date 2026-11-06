#include "internalTools.h"
#include "definitions.h"
#include <fstream>

ToolResult InternalTools::writeFile(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const auto content = args.at("content").get<std::string>();
        auto extension = args.at("extension").get<std::string>();
        const auto fileName = args.at("fileName").get<std::string>();

        if (content.size() > 200000)
            return {false, "", "Conteúdo muito grande"};

        if (!extension.empty() && extension[0] == '.')
            extension.erase(0, 1);
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        if (extension != "md")
            return {false, "", "Por segurança, só é permitido criar arquivos '.md' no momento"};

        const fs::path workspace = fs::weakly_canonical(Definitions::ensure_workspace());
        const fs::path target = fs::weakly_canonical(workspace / (fileName + "." + extension));

        const auto rel = target.lexically_relative(workspace);
        if (rel.empty() || *rel.begin() == "..")
            return {false, "", "Caminho fora do workspace: " + fileName};

        if (fs::exists(target))
            return {false, "", "Já existe: " + rel.string()};

        fs::create_directories(target.parent_path());

        std::ofstream file(target, std::ios::binary);
        if (!file.is_open())
            return {false, "", "Não foi possível abrir o arquivo para escrita"};

        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        file.close();

        if (file.fail())
            return {false, "", "Falha ao gravar o arquivo"};

        return {true, rel.string(), ""};
    }
    catch (const json::exception &e)
    {
        return {false, "", std::string("Argumentos inválidos: ") + e.what()};
    }
    catch (const fs::filesystem_error &e)
    {
        return {false, "", std::string("Erro de sistema de arquivos: ") + e.what()};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}