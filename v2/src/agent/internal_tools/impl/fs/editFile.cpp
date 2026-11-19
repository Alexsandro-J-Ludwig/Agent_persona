#include "internalTools.h"
#include "definitions.h"

#ifdef _WIN32
#define NOMINMAX
#endif

#include <fstream>
#include <format>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cstdint>
#include <atomic>
#include <iterator>
#include <system_error>
#include <Windows.h>

#include <system_error>

ToolResult InternalTools::editFile(const std::string &arguments)
{
    fs::path tempPath;

    try
    {
        const auto args = json::parse(arguments);

        const std::string fileName = args.at("file").get<std::string>();
        const std::string oldContent = args.at("oldContent").get<std::string>();
        const std::string newContent = args.at("newContent").get<std::string>();
        const size_t lineEdit = args.at("line").get<size_t>();

        if (fileName.empty() || oldContent.empty() || newContent.empty())
            return {false, "", "Argumentos não podem estar vazios"};

        if (oldContent.size() > 200000 || newContent.size() > 200000)
            return {false, "", "Conteúdo excede o limite de 200000 bytes"};

        if (lineEdit == 0)
            return {false, "", "A numeração das linhas começa em 1"};

        // 1. Define a raiz permitida.
        const fs::path workspace =
            Definitions::ensure_dev_workspace();

        // Não permite caminhos absolutos.
        const fs::path relativePath(fileName);

        if (relativePath.is_absolute() || relativePath.has_root_name())
            return {false, "", "Caminhos absolutos não são permitidos"};

        // 2. Resolve o caminho e verifica se permanece na raiz.
        const fs::path target =
            fs::weakly_canonical(workspace / relativePath);

        const auto rel = target.lexically_relative(workspace);

        if (rel.empty() || *rel.begin() == "..")
            return {false, "", "Caminho fora do workspace de desenvolvimento"};

        if (!fs::exists(target) || !fs::is_regular_file(target))
            return {false, "", "Arquivo não encontrado ou não é um arquivo regular"};

        // 3. Verifica a extensão.
        const std::string extension = target.extension().string();

        if (!Definitions::allowedExt.contains(extension))
            return {false, "", "Extensão de arquivo não permitida"};

        // 4. Lê o arquivo em modo binário para preservar as quebras de linha.
        std::ifstream in(target, std::ios::binary);

        if (!in.is_open())
            return {false, "", "Não foi possível abrir o arquivo"};

        std::string content(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());

        if (in.bad())
            return {false, "", "Erro durante a leitura do arquivo"};

        in.close();

        if (content.size() > 10 * 1024 * 1024)
            return {false, "", "Arquivo excede o limite de 10 MB"};

        // 5. Rejeita conteúdo binário.
        if (content.find('\0') != std::string::npos)
            return {false, "", "Arquivos binários não podem ser editados"};

        // 6. Detecta o padrão de quebra de linha.
        const std::string newline =
            content.find("\r\n") != std::string::npos ? "\r\n" : "\n";

        // 7. Separa as linhas preservando o conteúdo.
        std::vector<std::string> linhas;

        size_t start = 0;

        while (start < content.size())
        {
            size_t end = content.find('\n', start);

            if (end == std::string::npos)
            {
                linhas.push_back(content.substr(start));
                break;
            }

            size_t length = end - start;

            if (length > 0 && content[start + length - 1] == '\r')
                --length;

            linhas.push_back(content.substr(start, length));
            start = end + 1;
        }

        // 8. Valida a linha.
        if (lineEdit > linhas.size())
            return {
                false,
                "",
                std::format(
                    "Linha inexistente. Total de linhas: {}",
                    linhas.size())};

        const size_t index = lineEdit - 1;

        if (linhas[index] != oldContent)
            return {
                false,
                "",
                "O conteúdo atual da linha não corresponde a oldContent"};

        // 9. Aplica a edição em memória.
        linhas[index] = newContent;

        std::string updatedContent;

        for (size_t i = 0; i < linhas.size(); ++i)
        {
            updatedContent += linhas[i];

            if (i + 1 < linhas.size() ||
                (!content.empty() &&
                 (content.back() == '\n' || content.back() == '\r')))
            {
                updatedContent += newline;
            }
        }

        // 10. Cria arquivo temporário no mesmo diretório.
        static std::atomic_uint64_t tempCounter{0};

        tempPath = target;
        tempPath += std::format(
            ".tmp_{}",
            tempCounter.fetch_add(1));

        std::ofstream out(
            tempPath,
            std::ios::binary | std::ios::trunc);

        if (!out.is_open())
            return {false, "", "Não foi possível criar arquivo temporário"};

        out.write(
            updatedContent.data(),
            static_cast<std::streamsize>(updatedContent.size()));

        out.flush();

        if (!out.good())
        {
            out.close();
            fs::remove(tempPath);
            return {false, "", "Falha ao gravar arquivo temporário"};
        }

        out.close();

        if (out.fail())
        {
            fs::remove(tempPath);
            return {false, "", "Falha ao fechar arquivo temporário"};
        }

#ifdef _WIN32

        if (!MoveFileExW(
                tempPath.c_str(),
                target.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            const DWORD error = GetLastError();

            std::error_code ec;
            fs::remove(tempPath, ec);

            return {
                false,
                "",
                std::format(
                    "Falha ao substituir arquivo. Erro Windows {}: {}",
                    error,
                    std::system_category().message(
                        static_cast<int>(error)))};
        }

#else

        std::error_code ec;

        fs::rename(tempPath, target, ec);

        if (ec)
        {
            fs::remove(tempPath);

            return {
                false,
                "",
                "Falha ao substituir arquivo: " + ec.message()};
        }

#endif

        return {
            true,
            std::format(
                "Arquivo editado com sucesso.\n"
                "Linha modificada: {}\n\n"
                "Conteúdo anterior:\n{}\n\n"
                "Novo conteúdo:\n{}",
                lineEdit,
                oldContent,
                newContent),
            ""};
    }
    catch (const json::exception &e)
    {
        return {false, "", std::string("Argumentos inválidos: ") + e.what()};
    }
    catch (const fs::filesystem_error &e)
    {
        if (!tempPath.empty())
        {
            std::error_code ec;
            fs::remove(tempPath, ec);
        }

        return {false, "", std::string("Erro de filesystem: ") + e.what()};
    }
    catch (const std::exception &e)
    {
        if (!tempPath.empty())
        {
            std::error_code ec;
            fs::remove(tempPath, ec);
        }

        return {false, "", e.what()};
    }
}