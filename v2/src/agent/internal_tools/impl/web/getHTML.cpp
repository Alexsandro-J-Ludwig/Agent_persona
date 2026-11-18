#include "internalTools.h"
#include "httplib.h"

struct ParsedUrl
{
    std::string origin, path;
};

static ParsedUrl splitUrl(const std::string &url)
{
    const auto schemeEnd = url.find("://");
    const auto hostStart = schemeEnd == std::string::npos ? 0 : schemeEnd + 3;
    const auto pathStart = url.find('/', hostStart);
    if (pathStart == std::string::npos)
        return {url, "/"};
    return {url.substr(0, pathStart), url.substr(pathStart)};
}

inline std::string cleanHTML(std::string html)
{
    // 1. Remover comentários HTMLclear

    std::smatch m;
    if (std::regex_search(html, m,
                          std::regex(R"(<(main|article)\b[^>]*>([\s\S]*?)</\1\s*>)", std::regex::icase)))
        html = m[2].str();

    html = std::regex_replace(
        html, std::regex(R"(<!--[\s\S]*?-->)"), " ");

    // 2. Remover scripts e seus conteúdos
    html = std::regex_replace(
        html, std::regex(R"(<script\b[^>]*>[\s\S]*?</script\s*>)", std::regex::icase), " ");

    // 3. Remover estilos CSS e seus conteúdos
    html = std::regex_replace(
        html, std::regex(R"(<style\b[^>]*>[\s\S]*?</style\s*>)", std::regex::icase), " ");

    // 4. Remover SVGs inteiros
    html = std::regex_replace(
        html, std::regex(R"(<svg\b[^>]*>[\s\S]*?</svg\s*>)", std::regex::icase), " ");

    // 5. Remover conteúdo que não costuma ser texto principal
    html = std::regex_replace(
        html, std::regex(R"(<(noscript|template|iframe|object|embed|canvas)\b[^>]*>[\s\S]*?</\1\s*>)", std::regex::icase), " ");

    // 6. Remover formulários, campos e controles
    html = std::regex_replace(
        html, std::regex(R"(<(form|select|option|textarea|button|label|input)\b[^>]*>[\s\S]*?</\1\s*>)", std::regex::icase), " ");

    // Remover campos e controles autocontidos
    html = std::regex_replace(
        html, std::regex(R"(<(input|img|source|track|area|base|link|meta|param|wbr)\b[^>]*\/?>)", std::regex::icase), " ");

    // 7. Remover imagens e elementos de mídia
    html = std::regex_replace(
        html, std::regex(R"(<(picture|video|audio)\b[^>]*>[\s\S]*?</\1\s*>)", std::regex::icase), " ");

    // 8. Remover cabeçalho e rodapé global da página
    html = std::regex_replace(
        html, std::regex(R"(<(head|nav|footer)\b[^>]*>[\s\S]*?</\1\s*>)", std::regex::icase), " ");

    // 9. Remover títulos HTML h1-h6 e title
    html = std::regex_replace(
        html, std::regex(R"(<(h[1-6]|title)\b[^>]*>[\s\S]*?</\1\s*>)", std::regex::icase), " ");

    // 10. Remover tags HTML restantes
    html = std::regex_replace(
        html, std::regex(R"(<[^>]*>)"), " ");

    // 11. Normalizar espaços em branco
    html = std::regex_replace(
        html, std::regex(R"(\s+)"), " ");

    return html;
}

ToolResult InternalTools::getHTML(const std::string &arguments)
{
    try
    {
        const auto args = json::parse(arguments);
        const std::string url = args.at("url").get<std::string>();
        const auto [origin, path] = splitUrl(url);

        httplib::Client cli(origin);
        cli.set_connection_timeout(5);
        cli.set_read_timeout(10);
        cli.set_follow_location(true);
        cli.set_default_headers({{"User-Agent", "Mozilla/5.0"}});

        auto res = cli.Get(path);
        if (!res)
            return {false, "", "Falha na conexão: " + httplib::to_string(res.error())};
        if (res->status != 200)
            return {false, "", "HTTP " + std::to_string(res->status)};

        std::string html = res->body;
        if (html.size() > 500000)
            html.resize(500000);

        html = cleanHTML(html);

        constexpr std::size_t maxOut = 12000;
        bool truncated = false;
        if (html.size() > maxOut)
        {
            std::size_t n = maxOut;
            while (n > 0 && (static_cast<unsigned char>(html[n]) & 0xC0) == 0x80)
                --n;
            html.resize(n);
            truncated = true;
        }

        return {true, url + ": " + html + (truncated ? "\n[truncado]" : ""), ""};
    }
    catch (const std::exception &e)
    {
        return {false, "", e.what()};
    }
}