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

    constexpr std::string_view getRemoteHost()
    {
        return "https://api.incubebots.com";
    }

    constexpr std::string_view getLocalHost()
    {
        return "http://localhost:11434";
    }

    constexpr std::string_view getLocalDrive()
    {
        return "C:\\";
    }

    constexpr std::string_view getServerDrive()
    {
        if constexpr (ENVIRONMENT == Environment::Work)
            return "Z:\\";
        else
            return "";
    }
}

#endif