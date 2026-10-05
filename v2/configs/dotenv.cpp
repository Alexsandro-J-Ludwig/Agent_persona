#include "dotenv.h"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

namespace Config
{
    void DotEnv::load(const std::string &path)
    {
        std::ifstream file(path);

        if (!file.is_open())
            throw std::runtime_error(
                "Nao foi possivel abrir o arquivo .env");

        std::string line;

        while (std::getline(file, line))
        {
            // Ignora linha vazia e comentário
            if (line.empty() || line[0] == '#')
                continue;

            auto separator = line.find('=');

            if (separator == std::string::npos)
                continue;

            std::string key =
                line.substr(0, separator);

            std::string value =
                line.substr(separator + 1);

            // Remove aspas simples ou duplas
            if (value.size() >= 2)
            {
                if ((value.front() == '"' && value.back() == '"') ||
                    (value.front() == '\'' && value.back() == '\''))
                {
                    value =
                        value.substr(1, value.size() - 2);
                }
            }

#ifdef _WIN32
            _putenv_s(key.c_str(), value.c_str());
#else
            setenv(key.c_str(), value.c_str(), 1);
#endif
        }
    }

    std::string DotEnv::get(
        const std::string &key,
        const std::string &fallback)
    {
        const char *value =
            std::getenv(key.c_str());

        if (value == nullptr)
            return fallback;

        return value;
    }
}