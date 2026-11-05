#include "internalTools.h"
#include "definitions.h"

ToolResult InternalTools::makeDir(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const std::string rawPath = args.at("path").get<std::string>();

        const fs::path workspace = fs::weakly_canonical(Definitions::ensure_workspace());
        const fs::path target = fs::weakly_canonical(workspace / rawPath);

        const auto rel = target.lexically_relative(workspace);
        if (rel.empty() || *rel.begin() == "..")
            return {false, "Caminho fora do workspace: " + rawPath};

        if (fs::exists(target))
        {
            if (fs::is_directory(target))
                return {true, "Diretório já existe: " + rel.string()};
            return {false, "Já existe um arquivo com esse nome: " + rel.string()};
        }

        fs::create_directories(target);

        return {true, "Diretório criado: " + rel.string()};
    }
    catch (const json::exception &e)
    {
        return {false, std::string("Argumentos inválidos: ") + e.what()};
    }
    catch (const fs::filesystem_error &e)
    {
        return {false, std::string("Erro de sistema de arquivos: ") + e.what()};
    }
}