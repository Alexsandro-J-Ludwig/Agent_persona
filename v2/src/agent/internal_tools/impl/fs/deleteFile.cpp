
#include "internalTools.h"
#include "definitions.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

ToolResult InternalTools::deleteFile(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const std::string fileName =
            args.at("path").get<std::string>();

        if (fileName.empty())
            return {false, "", "O caminho não pode ser vazio"};

        const fs::path relativePath(fileName);

        if (relativePath.is_absolute() ||
            relativePath.has_root_name() ||
            relativePath.has_root_directory())
        {
            return {false, "", "Informe um caminho relativo ao workspace"};
        }

        for (const auto &part : relativePath)
        {
            if (part == "..")
                return {false, "", "Caminho fora do workspace"};
        }

        if (relativePath == "." || relativePath.empty())
            return {false, "", "Não é permitido excluir o workspace"};

        const fs::path workspace =
            fs::canonical(Definitions::ensure_workspace());

        const fs::path target =
            workspace / relativePath;

        // Não permite excluir links simbólicos.
        if (fs::is_symlink(fs::symlink_status(target)))
            return {false, "", "Não é permitido excluir links simbólicos"};

        if (!fs::exists(target))
            return {false, "", "Arquivo ou diretório não encontrado"};

        const fs::path canonicalTarget = fs::canonical(target);

        const auto relativeTarget =
            canonicalTarget.lexically_relative(workspace);

        if (relativeTarget.empty() ||
            relativeTarget.is_absolute() ||
            *relativeTarget.begin() == "..")
        {
            return {false, "", "Caminho fora do workspace"};
        }

        if (canonicalTarget == workspace)
            return {false, "", "Não é permitido excluir o workspace"};

        // Verifica links simbólicos dentro de diretórios.
        if (fs::is_directory(canonicalTarget))
        {
            for (fs::recursive_directory_iterator it(canonicalTarget), end;
                 it != end; ++it)
            {
                if (fs::is_symlink(it->symlink_status()))
                {
                    return {
                        false,
                        "",
                        "O diretório contém um link simbólico: " +
                            it->path().string()};
                }
            }
        }

        const bool isDirectory = fs::is_directory(canonicalTarget);

        std::uintmax_t removedCount = 0;

        if (isDirectory)
        {
            removedCount = fs::remove_all(canonicalTarget);
        }
        else
        {
            if (!fs::remove(canonicalTarget))
                return {false, "", "Não foi possível excluir o arquivo"};

            removedCount = 1;
        }

        return {
            true,
            std::string(isDirectory
                            ? "Diretório excluído com sucesso: "
                            : "Arquivo excluído com sucesso: ") +
                relativeTarget.string() +
                " (" + std::to_string(removedCount) +
                " itens removidos)",
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
            std::string("Erro de filesystem: ") + e.what()};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}
