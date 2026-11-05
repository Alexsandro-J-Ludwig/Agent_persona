#include "searxng.h"
#include "generalconfig.h"

#include <iostream>
#include <stdexcept>

Searxng::Searxng()
{
    if (Config::hasRemoteCredentials())
        remote = Config::createHttpClient({}, Config::Service::Search);

    local = Config::createLocalHttpClient({}, Config::Service::Search);
}

std::optional<Searxng::json> Searxng::tryGet(
    httplib::Client &client, const std::string &query, const char *origem)
{
    httplib::Params params = {
        {"q", query},
        {"format", "json"},
        {"language", "pt-BR"},
        {"safesearch", "1"},
    };

    httplib::Headers headers = {
        {"User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                       "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"},
        {"Accept", "application/json"},
        {"Accept-Language", "pt-BR,pt;q=0.9"},
    };

    auto res = client.Get("/search", params, headers);

    if (!res)
    {
        std::cerr << "[SearXNG] " << origem << ": sem resposta\n";
        return std::nullopt;
    }

    if (res->status < 200 || res->status >= 300)
    {
        std::cerr << "[SearXNG] " << origem << ": HTTP " << res->status << '\n';
        return std::nullopt;
    }

    // Se a Cloudflare devolver a página de login, vem HTML e o parse falha
    json data = json::parse(res->body, nullptr, /*allow_exceptions=*/false);
    if (data.is_discarded() || !data.is_object() || !data.contains("results"))
    {
        std::cerr << "[SearXNG] " << origem << ": resposta nao e JSON valido\n";
        return std::nullopt;
    }

    return data;
}

Searxng::json Searxng::search(const std::string &query, int maxResults)
{
    std::optional<json> data;

    if (remote)
        data = tryGet(*remote, query, "remoto");

    if (!data)
        data = tryGet(*local, query, "local");

    if (!data)
        throw std::runtime_error("Nenhuma fonte do SearXNG respondeu");

    json out = json::array();
    for (const auto &r : data->at("results"))
    {
        if (static_cast<int>(out.size()) >= maxResults)
            break;

        out.push_back({
            {"title", r.value("title", "")},
            {"url", r.value("url", "")},
            {"content", r.value("content", "")},
        });
    }
    return out;
}