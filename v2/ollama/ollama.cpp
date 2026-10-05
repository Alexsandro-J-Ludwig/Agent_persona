#include "ollama.h"

#include <stdexcept>

Ollama::Ollama()
    : client(Config::createHttpClient())
{
}

Ollama::json Ollama::generate(
    const Model &model,
    const std::string &prompt,
    bool thinking)
{
    json body = {
        {"model", model.name},
        {"prompt", prompt},
        {"stream", false},
        {"think", thinking},
        {"options", {{"num_ctx", model.num_ctx}}}};

    auto response = client->Post(
        "/api/generate",
        body.dump(),
        "application/json");

    if (!response)
    {
        throw std::runtime_error(
            "Falha ao conectar com o Ollama");
    }

    if (response->status < 200 || response->status >= 300)
    {
        throw std::runtime_error(
            "Ollama retornou HTTP " +
            std::to_string(response->status));
    }

    return json::parse(response->body);
}

Ollama::json Ollama::chat(
    const Model &model,
    const std::vector<json> &messages,
    const std::string &prompt,
    bool thinking)
{
    auto requestMessages = messages;
    requestMessages.push_back({{"role", "user"},
                               {"content", prompt}});

    json body = {
        {"model", model.name},
        {"messages", requestMessages},
        {"stream", false},
        {"keep_alive", model.keep_alive},
        {"think", thinking},
        {"options", {{"num_ctx", model.num_ctx}}}};

    auto response = client->Post(
        "/api/chat",
        body.dump(),
        "application/json");

    if (!response)
    {
        throw std::runtime_error(
            "Falha ao conectar com o Ollama");
    }

    if (response->status < 200 || response->status >= 300)
    {
        throw std::runtime_error(
            "Ollama retornou HTTP " +
            std::to_string(response->status));
    }

    return json::parse(response->body);
}

void Ollama::chatStream(
    const Model &model,
    const std::vector<json> &messages,
    const std::string &prompt,
    StreamCallback callback,
    bool thinking)
{
    auto requestMessages = messages;

    requestMessages.push_back({{"role", "user"},
                               {"content", prompt}});

    json body = {
        {"model", model.name},
        {"messages", requestMessages},
        {"stream", true},
        {"keep_alive", model.keep_alive},
        {"think", thinking},
        {"options", {{"num_ctx", model.num_ctx}}}};

    // POST usando ContentReceiver da httplib

    std::string buffer;

    auto receiver =
        [&buffer, callback](const char *data, std::size_t lenght)
    {
        buffer.append(data, lenght);

        std::size_t pos;

        while ((pos = buffer.find('\n')) != std::string::npos)
        {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (line.empty())
                continue;

            json chunk = json::parse(line);

            callback(chunk);
        }
        return true;
    };
}