#ifndef OLLAMA_H
#define OLLAMA_H

#include <memory>
#include <string>
#include <vector>
#include <json.hpp>
#include <array>
#include <functional>

#include "src/configs/models.h"
#include "src/configs/httplibconfig.h"

// Retorna texto e um array de chamadas de funcoes
struct ChatResult
{
    std::string response;
    nlohmann::json toolCalls = nlohmann::json::array();
};

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

    ChatResult chatStream(
        const Model &model,
        const std::vector<json> &history,
        StreamCallback callback,
        bool thinking = false);

private:
    std::unique_ptr<httplib::Client> client;
};

#endif // OLLAMA_H