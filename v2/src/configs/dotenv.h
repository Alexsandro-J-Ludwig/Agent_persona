#ifndef DOTENV_H
#define DOTENV_H

#include <string>

namespace Config
{
    class DotEnv
    {
    public:
        static void load(const std::string &path = ".env");

        static std::string get(
            const std::string &key,
            const std::string &fallback = "");
    };
}

#endif