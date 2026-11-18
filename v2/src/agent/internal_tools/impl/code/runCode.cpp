#include "internalTools.h"
#include "src/agent/internal_tools/impl/sandbox/sandbox.h"

#include <set>
#include <chrono>
#include <cstdint>

ToolResult InternalTools::runCode(const std::string &arguments)
{
    namespace fs = std::filesystem;

    try
    {
        const auto args = json::parse(arguments);

        if (!args.is_object() ||
            !args.contains("filePath") ||
            !args["filePath"].is_string())
        {
            return {
                false,
                "",
                "Argumentos inválidos. Informe filePath como string."};
        }

        const std::string filePath =
            args["filePath"].get<std::string>();

        if (filePath.empty() || filePath.find('\0') != std::string::npos)
        {
            return {
                false,
                "",
                "O caminho do arquivo não pode ser vazio."};
        }

        // Todos os limites são configuráveis por chamada.
        Sandbox::Config config;

        auto readInteger = [&](const char *key,
                               std::uint64_t defaultValue,
                               std::uint64_t minimum,
                               std::uint64_t maximum,
                               std::uint64_t &destination) -> bool
        {
            if (!args.contains(key))
            {
                destination = defaultValue;
                return true;
            }

            const auto &value = args[key];

            if (!value.is_number_integer() &&
                !value.is_number_unsigned())
            {
                return false;
            }

            try
            {
                const auto number = value.get<std::int64_t>();

                if (number < 0 ||
                    static_cast<std::uint64_t>(number) < minimum ||
                    static_cast<std::uint64_t>(number) > maximum)
                {
                    return false;
                }

                destination = static_cast<std::uint64_t>(number);
                return true;
            }
            catch (...)
            {
                return false;
            }
        };

        std::uint64_t timeout = 10;
        std::uint64_t memoryLimit = 512;
        std::uint64_t maxProcesses = 16;
        std::uint64_t maxOutput = 65536;

        if (!readInteger("timeout", 10, 1, 30, timeout) ||
            !readInteger("memoryLimitMB", 512, 16, 4096, memoryLimit) ||
            !readInteger("maxProcesses", 16, 1, 64, maxProcesses) ||
            !readInteger("maxOutputBytes", 65536, 1024, 1048576, maxOutput))
        {
            return {
                false,
                "",
                "Limites inválidos. Consulte os intervalos permitidos da ferramenta."};
        }

        config.timeout = std::chrono::seconds(timeout);
        config.memoryLimitMB = memoryLimit;
        config.maxProcesses = static_cast<std::uint32_t>(maxProcesses);
        config.maxOutputBytes = static_cast<std::size_t>(maxOutput);

        // Caminho recebido pelo modelo é sempre relativo.
        const fs::path requestedPath = fs::u8path(filePath);

        if (requestedPath.is_absolute() ||
            requestedPath.has_root_name() ||
            requestedPath.has_root_directory())
        {
            return {
                false,
                "",
                "Informe um caminho relativo à workspace."};
        }

        // Obtém um snapshot do diretório atual.
        fs::path baseDirectory;

        {
            std::lock_guard<std::mutex> lock(directoryMutex);
            baseDirectory = currentDirectory;
        }

        const fs::path candidate =
            (baseDirectory / requestedPath).lexically_normal();

        // Converte para caminho relativo à raiz da workspace.
        const fs::path relativePath =
            candidate.lexically_relative(rootDirectory);

        if (relativePath.empty() ||
            relativePath.is_absolute() ||
            *relativePath.begin() == ".." ||
            candidate == rootDirectory)
        {
            return {
                false,
                "",
                "O arquivo solicitado está fora da workspace."};
        }

        // Somente extensões executáveis suportadas.
        std::string extension = relativePath.extension().string();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        static const std::set<std::string> executableExtensions = {
            ".py",
            ".c",
            ".cpp",
            ".rs"};

        if (!executableExtensions.contains(extension))
        {
            return {
                false,
                "",
                "Extensão não executável. Suportadas: .py, .c, .cpp e .rs."};
        }

        // O backend recebe somente a raiz da workspace e o caminho relativo.
        config.workspace = rootDirectory;
        config.sourceFile = relativePath;

        // Execução isolada. Nenhum processo é iniciado diretamente aqui.
        const Sandbox::Result execution = Sandbox::run(config);

        std::string executionError;

        if (execution.timedOut)
        {
            executionError =
                "Tempo limite de execução excedido (" +
                std::to_string(timeout) + " segundos).";
        }
        else if (execution.exitCode != 0)
        {
            executionError =
                "O programa terminou com código de saída " +
                std::to_string(execution.exitCode) + ".";
        }

        if (!execution.error.empty())
        {
            if (!executionError.empty())
                executionError += "\n";

            executionError += execution.error;
        }

        if (execution.outputTruncated)
        {
            if (!executionError.empty())
                executionError += "\n";

            executionError +=
                "A saída foi truncada porque atingiu o limite configurado.";
        }

        return {
            execution.success,
            execution.output,
            executionError};
    }
    catch (const json::exception &e)
    {
        return {
            false,
            "",
            std::string("JSON inválido: ") + e.what()};
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
        return {
            false,
            "",
            std::string("Falha ao executar código: ") + e.what()};
    }
}
