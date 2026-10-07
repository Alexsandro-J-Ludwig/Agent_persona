#include "httplibconfig.h"
#include "generalconfig.h"
#include "dotenv.h"

#include <iostream>
#include <string>

namespace Config
{
    std::unique_ptr<httplib::Client> createHttpClient(
        const HttpProperties &properties)
    {
        std::string clientId;
        std::string clientSecret;

        try
        {
            DotEnv::load();

            clientId =
                DotEnv::get("CF-Access-Client-Id");

            clientSecret =
                DotEnv::get("CF-Access-Client-Secret");
        }
        catch (const std::exception &e)
        {
            std::cerr
                << "Arquivo .env nao encontrado. "
                << "Usando Ollama local.\n";
        }

        const bool remote =
            !clientId.empty() &&
            !clientSecret.empty();

        const auto host =
            remote
                ? std::string(getRemoteHost())
                : std::string(getLocalHost());

        auto client =
            std::make_unique<httplib::Client>(host);

        client->set_connection_timeout(
            properties.connection_timeout);

        client->set_read_timeout(
            properties.read_timeout);

        client->set_write_timeout(
            properties.write_timeout);

        if (remote)
        {
            client->set_default_headers({{"CF-Access-Client-Id", clientId},
                                         {"CF-Access-Client-Secret", clientSecret}});

            std::cout
                << "[HTTP] Ollama remoto: "
                << host << '\n';
        }
        else
        {
            std::cout
                << "[HTTP] Ollama local: "
                << host << '\n';
        }

        return client;
    }
}