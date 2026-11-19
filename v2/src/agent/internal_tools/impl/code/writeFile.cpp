
#include "internalTools.h"
#include "definitions.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

ToolResult InternalTools::writeFile(const std::string &arguments)
{
    fs::path target;

    try
    {
        const auto args = json::parse(arguments);

        const std::string content =
            args.at("content").get<std::string>();

        std::string extension =
            args.at("extension").get<std::string>();

        const std::string fileName =
            args.at("fileName").get<std::string>();

        // 1. Valida os argumentos.
        if (fileName.empty())
            return {false, "", "Nome do arquivo não pode ser vazio"};

        if (content.size() > 200000)
            return {false, "", "Conteúdo excede 200000 bytes"};

        if (fileName.find('\0') != std::string::npos ||
            extension.find('\0') != std::string::npos)
            return {false, "", "Argumentos contêm caracteres nulos"};

        if (extension.empty())
            return {false, "", "Extensão não pode ser vazia"};

        if (extension.front() == '.')
            extension.erase(0, 1);

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        // 2. Valida a extensão.
        if (!Definitions::allowedExt.contains("." + extension))
            return {false, "", "Tipo de arquivo não permitido"};

        // 3. Valida o caminho relativo.
        const fs::path relativePath(fileName + "." + extension);

        if (relativePath.empty() ||
            relativePath.is_absolute() ||
            relativePath.has_root_name() ||
            relativePath.has_root_directory())
            return {false, "", "Caminho absoluto não permitido"};

        for (const auto &component : relativePath)
        {
            const std::string name = component.string();

            if (name.empty() || name == "." || name == "..")
                return {false, "", "Componente de caminho inválido"};

            if (name.back() == '.' || name.back() == ' ')
                return {false, "", "Nome não pode terminar em ponto ou espaço"};

            for (unsigned char c : name)
            {
                if (c < 32 ||
                    c == '<' || c == '>' ||
                    c == ':' || c == '"' ||
                    c == '|' || c == '?' ||
                    c == '*' || c == '\\')
                    return {false, "", "Nome contém caracteres inválidos"};
            }

#ifdef _WIN32
            std::string base = name.substr(0, name.find('.'));

            std::transform(
                base.begin(), base.end(), base.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::toupper(c));
                });

            static const std::set<std::string> reserved = {
                "CON", "PRN", "AUX", "NUL",
                "COM1", "COM2", "COM3", "COM4", "COM5",
                "COM6", "COM7", "COM8", "COM9",
                "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
                "LPT6", "LPT7", "LPT8", "LPT9"};

            if (reserved.contains(base))
                return {false, "", "Nome reservado pelo Windows"};
#endif
        }

        // 4. Obtém o workspace.
        const fs::path workspace =
            fs::canonical(Definitions::ensure_workspace());

        const fs::path unresolved = workspace / relativePath;

        fs::create_directories(unresolved.parent_path());

        // Canonicaliza o diretório pai para detectar links simbólicos.
        const fs::path parent =
            fs::canonical(unresolved.parent_path());

        const auto parentRel = parent.lexically_relative(workspace);

        if (parentRel.empty() ||
            parentRel.is_absolute() ||
            *parentRel.begin() == "..")
            return {false, "", "Diretório fora do workspace"};

        target = parent / unresolved.filename();

        // 5. Criação exclusiva: nunca sobrescreve arquivos.
#ifdef _WIN32

        HANDLE handle = CreateFileW(
            target.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            const DWORD error = GetLastError();

            if (error == ERROR_FILE_EXISTS ||
                error == ERROR_ALREADY_EXISTS)
                return {false, "", "O arquivo já existe"};

            return {
                false,
                "",
                "Falha ao criar arquivo. Erro Windows: " +
                    std::to_string(error)};
        }

        bool success = true;
        size_t offset = 0;

        while (offset < content.size())
        {
            const DWORD chunk = static_cast<DWORD>(
                std::min<size_t>(
                    content.size() - offset,
                    MAXDWORD));

            DWORD written = 0;

            if (!WriteFile(
                    handle,
                    content.data() + offset,
                    chunk,
                    &written,
                    nullptr) ||
                written == 0)
            {
                success = false;
                break;
            }

            offset += written;
        }

        if (success && !FlushFileBuffers(handle))
            success = false;

        if (!CloseHandle(handle))
            success = false;

        if (!success)
        {
            std::error_code ec;
            fs::remove(target, ec);

            return {false, "", "Falha durante a gravação do arquivo"};
        }

#else

        const int fd = ::open(
            target.c_str(),
            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
            0600);

        if (fd == -1)
        {
            if (errno == EEXIST)
                return {false, "", "O arquivo já existe"};

            return {
                false,
                "",
                "Falha ao criar arquivo: " +
                    std::error_code(errno, std::generic_category()).message()};
        }

        bool success = true;
        size_t offset = 0;

        while (offset < content.size())
        {
            const ssize_t written = ::write(
                fd,
                content.data() + offset,
                content.size() - offset);

            if (written < 0)
            {
                if (errno == EINTR)
                    continue;

                success = false;
                break;
            }

            if (written == 0)
            {
                success = false;
                break;
            }

            offset += static_cast<size_t>(written);
        }

        if (success && ::fsync(fd) != 0)
            success = false;

        if (::close(fd) != 0)
            success = false;

        if (!success)
        {
            std::error_code ec;
            fs::remove(target, ec);

            return {false, "", "Falha durante a gravação do arquivo"};
        }

#endif

        return {
            true,
            "Arquivo criado com sucesso: " + target.string(),
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
