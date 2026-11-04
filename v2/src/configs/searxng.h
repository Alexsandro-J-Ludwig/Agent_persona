#ifndef SEARXNG_H
#define SEARXNG_H

#include "httplibconfig.h"
#include <json.hpp>
#include <memory>
#include <optional>
#include <string>

class Searxng
{
public:
    using json = nlohmann::json;

    Searxng();

    // Retorna [{title, url, content}, ...]
    json search(const std::string &query, int maxResults = 10);

private:
    std::unique_ptr<httplib::Client> remote; // nullptr se não houver credenciais
    std::unique_ptr<httplib::Client> local;

    static std::optional<json> tryGet(httplib::Client &client,
                                      const std::string &query,
                                      const char *origem);
};

#endif