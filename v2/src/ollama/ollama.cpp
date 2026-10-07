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

std::string Ollama::chatStream(
    const Model &model,
    const std::vector<json> &messages,
    StreamCallback callback,
    bool thinking)
{
    json body = {
        {"model", model.name},
        {"messages", messages},
        {"stream", true},
        {"keep_alive", model.keep_alive},
        {"think", thinking},
        {"options", {{"num_ctx", model.num_ctx}}}};

    std::string buffer;
    std::string responseText;

    auto receiver =
        [&buffer, &responseText, callback](const char *data, std::size_t lenght)
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

            std::string content =
                chunk["message"].value("content", "");

            responseText += content;

            callback(content);
        }

        return true;
    };

    httplib::Headers headers;

    auto response = client->Post(
        "/api/chat",
        headers,
        body.dump(),
        "application/json",
        receiver);

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

    if (!buffer.empty())
    {
        json chunk = json::parse(buffer);

        if (chunk.contains("message"))
        {
            std::string content =
                chunk["message"].value("content", "");

            responseText += content;
        }

        callback(chunk);
    }

    return responseText;
}