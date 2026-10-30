#include "ollama.h"

#include <iostream>
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
        {"options", {{"num_ctx", model.num_ctx}, {"num_predict", model.num_predict}}}};

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
        std::cerr << "\n========== ERRO OLLAMA ==========\n"
                  << "[Ollama] HTTP: " << response->status << '\n'
                  << "[Ollama] BODY: " << response->body << '\n'
                  << "[Ollama] Origem: Ollama.cpp\n"
                  << "================================\n"
                  << std::flush;

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
        {"options", {{"num_ctx", model.num_ctx}, {"num_predict", model.num_predict}}}};

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
        std::cerr << "\n========== ERRO OLLAMA ==========\n"
                  << "[Ollama] HTTP: " << response->status << '\n'
                  << "[Ollama] BODY: " << response->body << '\n'
                  << "[Ollama] Origem: Ollama.cpp\n"
                  << "================================\n"
                  << std::flush;

        throw std::runtime_error(
            "Ollama retornou HTTP " +
            std::to_string(response->status));
    }

    return json::parse(response->body);
}

ChatResult Ollama::chatStream(
    const Model &model,
    const std::vector<json> &messages,
    StreamCallback callback,
    ThinkCallback thinkCallback,
    bool thinking,
    const json &tools)
{
    json body = {
        {"model", model.name},
        {"messages", messages},
        {"stream", true},
        {"keep_alive", model.keep_alive},
        {"think", thinking},
        {"options", {{"num_ctx", model.num_ctx}, {"num_predict", model.num_predict}}}};

    if (!tools.empty())
        body["tools"] = tools;

    std::string buffer;  // bytes recebidos que ainda não formaram uma linha completa
    std::string rawHead; // primeiros bytes da resposta, só para diagnosticar erro HTTP
    std::string responseText;
    json toolCalls = json::array();
    std::string responseThink;

    constexpr std::size_t kRawHeadMax = 4096;

    auto processLine = [&](const std::string &line)
    {
        if (line.empty())
            return;

        json chunk = json::parse(line, nullptr, /*allow_exceptions=*/false);
        if (chunk.is_discarded() || !chunk.is_object() || !chunk.contains("message"))
            return;

        const auto &message = chunk.at("message");

        if (message.contains("tool_calls") && message.at("tool_calls").is_array())
        {
            for (const auto &call : message.at("tool_calls"))
                toolCalls.push_back(call);
        }

        const std::string content = message.value("content", "");
        if (!content.empty())
        {
            responseText += content;
            if (callback)
                callback(content);
        }

        const std::string think = message.value("think", "");
        if (!think.empty())
        {
            responseThink += think;
            if (thinkCallback)
                thinkCallback(think);
        }
    };

    auto receiver = [&](const char *data, std::size_t length)
    {
        if (rawHead.size() < kRawHeadMax)
            rawHead.append(data, std::min(length, kRawHeadMax - rawHead.size()));

        buffer.append(data, length);

        std::size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos)
        {
            processLine(buffer.substr(0, pos));
            buffer.erase(0, pos + 1);
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
        std::cerr << "\n========== ERRO OLLAMA ==========\n"
                  << "[Ollama] HTTP: " << response->status << '\n'
                  << "[Ollama] BODY: " << (rawHead.empty() ? response->body : rawHead) << '\n'
                  << "[Ollama] Origem: Ollama.cpp (chatStream)\n"
                  << "================================\n"
                  << std::flush;

        throw std::runtime_error(
            "Ollama retornou HTTP " +
            std::to_string(response->status));
    }

    processLine(buffer);
    buffer.clear();

    return {responseText, toolCalls, responseThink};
}