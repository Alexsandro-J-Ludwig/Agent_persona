
#include "internalTools.h"
#include "definitions.h"

#include <string>
#include <system_error>

ToolResult InternalTools::makeDir(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const std::string rawPath = args.at("path").get<std::string>();

        if (rawPath.empty())
            return {false, "", "Caminho não pode ser vazio"};

        const fs::path relativePath(rawPath);

        if (relativePath.is_absolute() ||
            relativePath.has_root_name() ||
            relativePath.has_root_directory())
            return {false, "", "Caminhos absolutos não são permitidos"};

        for (const auto &component : relativePath)
        {
            if (component.empty() ||
                component == "." ||
                component == "..")
                return {false, "", "Caminho contém componente inválido"};
        }

        // 1. Obtém a raiz do workspace.
        const fs::path workspace =
            fs::canonical(Definitions::ensure_workspace());

        const fs::path unresolved = workspace / relativePath;

        // 2. Resolve o caminho.
        const fs::path target = fs::weakly_canonical(unresolved);

        const auto rel = target.lexically_relative(workspace);

        if (rel.empty() ||
            rel.is_absolute() ||
            *rel.begin() == "..")
            return {false, "", "Caminho fora do workspace: " + rawPath};

        // 3. Verifica se já existe.
        if (fs::exists(target))
        {
            if (fs::is_directory(target))
                return {
                    true,
                    "Diretório já existe: " + rel.string(),
                    ""};

            return {
                false,
                "",
                "Já existe um arquivo com esse nome: " + rel.string()};
        }

        // 4. Cria os diretórios.
        const bool created = fs::create_directories(target);

        // 5. Confirma o resultado.
        if (!fs::is_directory(target))
            return {
                false,
                "",
                "Não foi possível criar o diretório"};

        return {
            true,
            created
                ? "Diretório criado: " + rel.string()
                : "Diretório já existe: " + rel.string(),
            ""};
    }
    catch (const json::exception &e)
    {
        return {
            false,
            "",
            std::string("Argumentos inválidos: ") + e.what()};
    }
    catch (const fs::filesystem_error &e)
    {
        return {
            false,
            "",
            std::string("Erro de sistema de arquivos: ") + e.what()};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}
