#ifndef SANDBOX_H
#define SANDBOX_H

#include <string>
#include <filesystem>
#include <chrono>
#include <cstdint>

namespace Sandbox
{
    struct Config
    {
        std::filesystem::path workspace;
        std::filesystem::path sourceFile;

        // Tempo máximo total de compilação e execução.
        std::chrono::seconds timeout{10};

        // Limites de recursos.
        std::uint64_t memoryLimitMB = 512;
        std::uint32_t maxProcesses = 16;

        // Limite máximo combinado de stdout e stderr.
        std::size_t maxOutputBytes = 64 * 1024;
    };

    struct Result
    {
        bool success = false;
        int exitCode = -1;

        std::string output;
        std::string error;

        bool timedOut = false;
        bool outputTruncated = false;
    };

    // Executa um arquivo dentro de uma sandbox isolada.
    // A implementação concreta é selecionada conforme o sistema operacional.
    Result run(const Config &config);
}

#endif