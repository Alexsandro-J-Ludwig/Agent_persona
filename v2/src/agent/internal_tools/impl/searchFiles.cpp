#include "internalTools.h"

ToolResult InternalTools::searchFiles(const std::string &arguments)
{
    std::string term;

    try
    {
        term = json::parse(arguments)
                   .at("termo")
                   .get<std::string>();
    }
    catch (const std::exception &)
    {
        return {
            false, "",
            "Argumentos inválidos. Envie {\"termo\":\"texto\"}."};
    }

    if (term.empty())
        return {false, "", "Termo de pesquisa vazio."};

#ifdef _WIN32
    const fs::path termPath = fs::u8path(term);
    const std::wstring needle = termPath.wstring();

    const DWORD drives = GetLogicalDrives();

    if (drives == 0)
        return {false, "", "Não foi possível enumerar os discos."};

    std::string result;
    std::size_t found = 0;

    constexpr std::size_t maxStoredResults = 100;

    for (int i = 0; i < 26; ++i)
    {
        if (!(drives & (1UL << i)))
            continue;

        const wchar_t letter = static_cast<wchar_t>(L'A' + i);
        const std::wstring root =
            std::wstring(1, letter) + L":\\";

        const UINT driveType = GetDriveTypeW(root.c_str());

        if (driveType == DRIVE_NO_ROOT_DIR ||
            driveType == DRIVE_UNKNOWN ||
            driveType == DRIVE_CDROM)
        {
            continue;
        }

        std::vector<fs::path> pending{fs::path(root)};
        while (!pending.empty())
        {
            fs::path directory = std::move(pending.back());
            pending.pop_back();
            std::error_code ec;
            fs::directory_iterator it(
                directory, fs::directory_options::skip_permission_denied, ec);
            const fs::directory_iterator end;
            if (ec)
                continue;

            while (it != end)
            {
                const fs::path path = it->path();
                if (containsInsensitive(path.filename().wstring(), needle))
                {
                    const auto pathUtf8 = path.u8string();
                    result.append(reinterpret_cast<const char *>(pathUtf8.data()),
                                  pathUtf8.size());
                    result += '\n';
                    if (++found >= maxStoredResults)
                    {
                        result.pop_back();
                        return {true, result, ""};
                    }
                }

                const DWORD attributes = GetFileAttributesW(path.c_str());
                if (attributes != INVALID_FILE_ATTRIBUTES &&
                    (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    !(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                    pending.push_back(path);

                it.increment(ec);
                if (ec)
                    break;
            }
        }
    }

    if (found == 0)
        return {false, "", "Nenhum arquivo encontrado nos diretórios acessíveis."};

    if (!result.empty() && result.back() == '\n')
        result.pop_back();

    return {true, result, ""};

#else
    std::string termo_str;
    try
    {
        termo_str = json::parse(arguments).at("termo").get<std::string>();
    }
    catch (const std::exception &)
    {
        return {false, "", "Argumentos inválidos. Envie {\"termo\": \"texto\"}."};
    }

    if (termo_str.size() < 3 ||
        termo_str.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ._-") != std::string::npos)
        return {false, "", "Termo inválido: use 3+ caracteres, só letras, números, espaço, '.', '-' ou '_'."};

    std::string comando =
        "find / \\( -path /proc -o -path /sys -o -path /dev -o -path /run \\) -prune -o -iname '*" + termo_str +
        "*' -print 2>/dev/null | head -n 10";

    std::array<char, 1128> buffer;
    std::string resultado;

    FILE *pipe = popen(comando.c_str(), "r");
    if (!pipe)
        return {
            false,
            "",
            "Erro ao tentar executar o comando no sistema"};

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        resultado += buffer.data();
    }
    pclose(pipe);

    if (!resultado.empty() && resultado.back() == '\n')
        resultado.pop_back();

    if (resultado.empty())
    {
        return {
            false,
            "",
            "Nenhum arquivo encontrado nos diretórios acessíveis."};
    }

    return {
        true,
        resultado,
        ""};
#endif
}