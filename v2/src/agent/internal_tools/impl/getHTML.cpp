#include "internalTools.h"
#include "http.h"

inline std::string cleanHTML(std::string html)
{
    // 1. Remover comentários HTML
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
        std::string url =
            args.at("url").get<std::string>();

        httplib::Client cli(url);

        cli.set_connection_timeout(5);

        std::string html;

        if (auto res = cli.Get("/"))
        {
            if (res->status == 200)
            {
                html = res->body;
            }
        }

        html = cleanHTML(html);

        return {
            true,
            url + ":" + html,
            ""};
    }
    catch (const std::exception &e)
    {
        return {
            false,
            "",
            e.what()};
    }
}
