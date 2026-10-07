#ifndef OLLAMA_H
#define OLLAMA_H

#include <memory>
#include <string>
#include <vector>
#include <json.hpp>
#include <array>
#include <functional>

#include "../configs/models.h"
#include "../configs/httplibconfig.h"

class Ollama
{
public:
    Ollama();
    using json = nlohmann::json;
    using StreamCallback =
        std::function<void(const std::string &)>;

    json generate(
        const Model &model,
        const std::string &prompt,
        bool thinking = false);

    json chat(
        const Model &model,
        const std::vector<json> &history,
        const std::string &prompt,
        bool thinking = false);

    std::string chatStream(
        const Model &model,
        const std::vector<json> &history,
        StreamCallback callback,
        bool thinking = false);

private:
    std::unique_ptr<httplib::Client> client;
};

#endif // OLLAMA_H