#ifndef HTTPCONFIG_H
#define HTTPCONFIG_H

#include <httplib.h>
#include <memory>
#include <cstdlib>

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
        const HttpProperties &properties = {});
}

#endif // HTTPCONFIG_H