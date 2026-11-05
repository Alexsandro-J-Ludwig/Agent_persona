#include "internalTools.h"

namespace
{
    // Compara sem diferenciar maiúsculas e minúsculas (ASCII).
    bool containsInsensitive(const std::wstring &text, const std::wstring &needle)
    {
        if (needle.empty())
            return true;

        auto lower = [](wchar_t c)
        {
            return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c + (L'a' - L'A')) : c;
        };

        auto it = std::search(
            text.begin(), text.end(),
            needle.begin(), needle.end(),
            [&](wchar_t a, wchar_t b)
            { return lower(a) == lower(b); });

        return it != text.end();
    }
}

namespace
{
    const std::set<std::string> IGNORED_DIRS = {
        "build", "_deps", "CMakeFiles", ".git", ".vs", ".vscode",
        "node_modules", "__pycache__", "out", ".cache"};
}

namespace
{
    constexpr std::size_t MAX_RESULTS = 30;
    constexpr auto TIME_BUDGET = std::chrono::seconds(5);

    bool validTerm(const std::string &t)
    {
        return t.size() >= 3 &&
               t.find_first_not_of(
                   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ._-áéíóúâêôãõçÁÉÍÓÚÂÊÔÃÕÇ") == std::string::npos;
    }
}

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

    if (!validTerm(term))
        return {
            false,
            "",
            "Não é um termo valido"};

#ifdef _WIN32
    const std::wstring needle =
        fs::path(std::u8string(reinterpret_cast<const char8_t *>(term.data()), term.size())).wstring();

    const DWORD drives = GetLogicalDrives();
    if (drives == 0)
        return {false, "", "Não foi possível enumerar os discos."};

    std::vector<std::string> hits;
    bool truncated = false;
    const auto deadline = std::chrono::steady_clock::now() + TIME_BUDGET;

    for (int i = 0; i < 26 && !truncated; ++i)
    {
        if (!(drives & (1UL << i)))
            continue;

        const std::wstring root = std::wstring(1, static_cast<wchar_t>(L'A' + i)) + L":\\";
        const UINT driveType = GetDriveTypeW(root.c_str());
        if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_UNKNOWN || driveType == DRIVE_CDROM)
            continue;

        std::vector<fs::path> pending{root};

        while (!pending.empty() && !truncated)
        {
            if (std::chrono::steady_clock::now() > deadline)
            {
                truncated = true;
                break;
            }

            fs::path directory = std::move(pending.back());
            pending.pop_back();

            std::error_code ec;
            fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec);
            if (ec)
                continue;

            for (const fs::directory_iterator end; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    break;
                }

                const fs::path path = it->path();
                const std::string name = path.filename().string();

                if (containsInsensitive(path.filename().wstring(), needle))
                {
                    if (hits.size() == MAX_RESULTS)
                    {
                        truncated = true;
                        break;
                    }
                    const auto utf8 = path.u8string();
                    hits.emplace_back(reinterpret_cast<const char *>(utf8.data()), utf8.size());
                }

                const DWORD attr = GetFileAttributesW(path.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES &&
                    (attr & FILE_ATTRIBUTE_DIRECTORY) &&
                    !(attr & FILE_ATTRIBUTE_REPARSE_POINT) &&
                    !IGNORED_DIRS.count(name))
                {
                    pending.push_back(path);
                }
            }
        }
    }

    if (hits.empty())
        return {false, "", "Nenhum arquivo encontrado nos diretórios acessíveis."};

    std::string out;
    for (const auto &h : hits)
        out += h + '\n';

    if (truncated)
        out += "... (busca interrompida pelo limite de resultados ou de tempo; refine o termo)";
    else
        out.pop_back();

    return {true, out, ""};

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

    const std::string comando =
        "find / \\( -path /proc -o -path /sys -o -path /dev -o -path /run \\) -prune -o "
        "\\( -type d \\( -name build -o -name _deps -o -name CMakeFiles -o -name .git "
        "-o -name node_modules -o -name .vs \\) -prune \\) -o "
        "-iname '*" +
        term + "*' -print 2>/dev/null | head -n " + std::to_string(MAX_RESULTS + 1);

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