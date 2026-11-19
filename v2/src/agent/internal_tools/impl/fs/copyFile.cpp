
#include "internalTools.h"
#include "definitions.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace
{
    bool isInside(const fs::path &path, const fs::path &parent)
    {
        const auto relative = path.lexically_relative(parent);

        return !relative.empty() &&
               !relative.is_absolute() &&
               *relative.begin() != "..";
    }
}

ToolResult InternalTools::copyFile(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);

        const std::string sourceName =
            args.at("source").get<std::string>();

        const std::string destinationName =
            args.at("destination").get<std::string>();

        if (sourceName.empty() || destinationName.empty())
            return {false, "", "Origem e destino são obrigatórios"};

        const fs::path sourceInput(sourceName);
        const fs::path destinationRelative(destinationName);

        if (sourceInput.native().find(
                fs::path::string_type(1, fs::path::value_type{})) !=
            fs::path::string_type::npos)
        {
            return {false, "", "Caminho de origem inválido"};
        }

        // O destino obrigatoriamente deve ser relativo ao workspace.
        if (destinationRelative.is_absolute() ||
            destinationRelative.has_root_name() ||
            destinationRelative.has_root_directory())
        {
            return {false, "", "O destino deve ser relativo ao workspace"};
        }

        for (const auto &part : destinationRelative)
        {
            if (part == "..")
                return {false, "", "Destino fora do workspace"};
        }

        if (destinationRelative.empty() ||
            destinationRelative.filename().empty())
        {
            return {false, "", "Destino inválido"};
        }

        // Resolve a origem.
        if (!fs::exists(sourceInput))
            return {false, "", "Origem não encontrada"};

        if (fs::is_symlink(fs::symlink_status(sourceInput)))
            return {false, "", "A origem não pode ser um link simbólico"};

        const fs::path source = fs::canonical(sourceInput);

        const bool isDirectory = fs::is_directory(source);
        const bool isFile = fs::is_regular_file(source);

        if (!isDirectory && !isFile)
            return {false, "", "A origem não é um arquivo ou diretório válido"};

        // Resolve o workspace.
        const fs::path workspace =
            fs::canonical(Definitions::ensure_workspace());

        const fs::path destination =
            (workspace / destinationRelative).lexically_normal();

        if (!isInside(destination, workspace))
            return {false, "", "Destino fora do workspace"};

        // Impede copiar o workspace para dentro de si mesmo.
        if (isDirectory &&
            (isInside(workspace, source) ||
             isInside(destination, source)))
        {
            return {
                false,
                "",
                "Não é permitido copiar um diretório para dentro dele mesmo "
                "ou de um diretório que contenha o workspace"};
        }

        // Verifica links simbólicos e tipos especiais antes da cópia.
        if (isDirectory)
        {
            for (fs::recursive_directory_iterator it(source), end;
                 it != end; ++it)
            {
                const auto status = it->symlink_status();

                if (fs::is_symlink(status))
                    return {
                        false,
                        "",
                        "A origem contém um link simbólico: " +
                            it->path().string()};

                if (!fs::is_directory(status) &&
                    !fs::is_regular_file(status))
                {
                    return {
                        false,
                        "",
                        "A origem contém um arquivo de tipo não suportado: " +
                            it->path().string()};
                }
            }
        }

        // Cria os diretórios pais do destino.
        const fs::path destinationParent =
            destination.parent_path();

        fs::create_directories(destinationParent);

        const fs::path canonicalParent =
            fs::canonical(destinationParent);

        if (!isInside(canonicalParent, workspace))
            return {false, "", "Diretório de destino fora do workspace"};

        const fs::path finalDestination =
            canonicalParent / destination.filename();

        // Não sobrescreve arquivos ou diretórios existentes.
        if (fs::exists(finalDestination) ||
            fs::is_symlink(fs::symlink_status(finalDestination)))
        {
            return {
                false,
                "",
                "Já existe um arquivo ou diretório no destino"};
        }

        if (isFile)
        {
            fs::copy_file(
                source,
                finalDestination,
                fs::copy_options::none);
        }
        else
        {
            // Cria a raiz do diretório importado.
            fs::create_directory(finalDestination);

            for (fs::recursive_directory_iterator it(source), end;
                 it != end; ++it)
            {
                const fs::path relative =
                    it->path().lexically_relative(source);

                const fs::path target =
                    finalDestination / relative;

                if (it->is_directory())
                {
                    fs::create_directory(target);
                }
                else if (it->is_regular_file())
                {
                    fs::copy_file(
                        it->path(),
                        target,
                        fs::copy_options::none);
                }
            }
        }

        return {
            true,
            "Cópia concluída com sucesso: " +
                finalDestination.string(),
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
