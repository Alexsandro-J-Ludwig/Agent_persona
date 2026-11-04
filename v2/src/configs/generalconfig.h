#ifndef GENERAL_CONFIG_H
#define GENERAL_CONFIG_H

#include <string_view>

namespace Config
{
    enum class Environment
    {
        Home,
        Work,
    };

    inline constexpr Environment ENVIRONMENT =
        Environment::Work;

    enum class Service
    {
        Ollama,
        Search
    };

    constexpr std::string_view getRemoteHost(Service service = Service::Ollama)
    {
        return service == Service::Search
                   ? "https://search.incubebots.com"
                   : "https://api.incubebots.com";
    }

    constexpr std::string_view getLocalHost(Service service = Service::Ollama)
    {
        // 8888 = porta publicada no host pelo docker-compose do SearXNG
        return service == Service::Search
                   ? "http://localhost:8888"
                   : "http://localhost:11434";
    }

    constexpr std::string_view getLocalDrive()
    {
        return "C:\\";
    }
}

#endif