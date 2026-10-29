#include "httplibconfig.h"
#include "generalconfig.h"
#include "dotenv.h"

#include <iostream>
#include <string>

namespace Config
{
    namespace
    {
        struct Credentials
        {
            std::string id;
            std::string secret;
            bool valid() const { return !id.empty() && !secret.empty(); }
        };

        Credentials loadCredentials()
        {
            try
            {
                DotEnv::load();
                return {DotEnv::get("CF-Access-Client-Id"),
                        DotEnv::get("CF-Access-Client-Secret")};
            }
            catch (const std::exception &)
            {
                return {};
            }
        }

        void applyTimeouts(httplib::Client &c, const HttpProperties &p)
        {
            c.set_connection_timeout(p.connection_timeout);
            c.set_read_timeout(p.read_timeout);
            c.set_write_timeout(p.write_timeout);
        }
    }

    bool hasRemoteCredentials()
    {
        return loadCredentials().valid();
    }

    std::unique_ptr<httplib::Client> createLocalHttpClient(
        const HttpProperties &properties, Service service)
    {
        const std::string host(getLocalHost(service));
        auto client = std::make_unique<httplib::Client>(host);
        applyTimeouts(*client, properties);
        std::cout << "[HTTP] Local: " << host << '\n';
        return client;
    }

    std::unique_ptr<httplib::Client> createHttpClient(
        const HttpProperties &properties, Service service)
    {
        const auto creds = loadCredentials();

        if (!creds.valid())
        {
            std::cerr << "Credenciais Cloudflare nao encontradas. Usando local.\n";
            return createLocalHttpClient(properties, service);
        }

        const std::string host(getRemoteHost(service));
        auto client = std::make_unique<httplib::Client>(host);

        applyTimeouts(*client, properties);
        client->set_default_headers({{"CF-Access-Client-Id", creds.id},
                                     {"CF-Access-Client-Secret", creds.secret}});

        std::cout << "[HTTP] Remoto: " << host << '\n';
        return client;
    }
}