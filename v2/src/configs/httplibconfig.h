#ifndef HTTPCONFIG_H
#define HTTPCONFIG_H

#include <httplib.h>
#include <memory>
#include <cstdlib>
#include "generalconfig.h"

namespace Config
{
    struct HttpProperties
    {
        int connection_timeout = 10;
        int read_timeout = 300;
        int write_timeout = 30;
        std::string default_headers[2];
    };

    std::unique_ptr<httplib::Client> createHttpClient(
        const HttpProperties &properties = {},
        Service service = Service::Ollama);

    std::unique_ptr<httplib::Client> createLocalHttpClient(
        const HttpProperties &properties = {},
        Service service = Service::Ollama);

    bool hasRemoteCredentials();
}

#endif // HTTPCONFIG_H