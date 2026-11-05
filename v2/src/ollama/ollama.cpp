#include "ollama.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

Ollama::Ollama()
    : client(Config::createHttpClient())
{
    client->set_read_timeout(600, 0);
    client->set_write_timeout(60, 0);
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
        throw std::runtime_error("Falha ao conectar com o Ollama");

    if (response->status < 200 || response->status >= 300)
    {
        std::cerr << "\n========== ERRO OLLAMA ==========\n"
                  << "[Ollama] HTTP: " << response->status << '\n'
                  << "[Ollama] BODY: " << response->body << '\n'
                  << "[Ollama] Origem: Ollama.cpp\n"
                  << "================================\n"
                  << std::flush;

        throw std::runtime_error(
            "Ollama retornou HTTP " + std::to_string(response->status));
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
        throw std::runtime_error("Falha ao conectar com o Ollama");

    if (response->status < 200 || response->status >= 300)
    {
        std::cerr << "\n========== ERRO OLLAMA ==========\n"
                  << "[Ollama] HTTP: " << response->status << '\n'
                  << "[Ollama] BODY: " << response->body << '\n'
                  << "[Ollama] Origem: Ollama.cpp\n"
                  << "================================\n"
                  << std::flush;

        throw std::runtime_error(
            "Ollama retornou HTTP " + std::to_string(response->status));
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
    constexpr int kMaxContinuations = 3;
    constexpr std::size_t kRawHeadMax = 4096;

    int continuations = 0;

    std::string responseText;
    std::string responseThink;
    json toolCalls = json::array();
    json conversation = messages;

    while (true)
    {
        bool truncated = false;
        std::string doneReason;
        std::string currentGeneration;
        std::string buffer;
        std::string rawHead;

        json body = {
            {"model", model.name},
            {"messages", conversation},
            {"stream", true},
            {"keep_alive", model.keep_alive},
            {"think", thinking},
            {"options", {{"num_ctx", model.num_ctx}, {"num_predict", model.num_predict}}}};

        if (!tools.empty())
            body["tools"] = tools;

        auto processLine = [&](const std::string &line)
        {
            if (line.empty())
                return;

            json chunk = json::parse(line, nullptr, /*allow_exceptions=*/false);
            if (chunk.is_discarded() || !chunk.is_object() || !chunk.contains("message"))
                return;

            if (chunk.value("done", false))
            {
                doneReason = chunk.value("done_reason", "");
                truncated = (doneReason == "length");
            }

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
                currentGeneration += content;
                if (callback)
                    callback(content);
            }

            const std::string think = message.value("thinking", "");
            if (!think.empty())
            {
                responseThink += think;
                if (thinkCallback)
                    thinkCallback(think);
            }

            std::cerr << "[PARSED] content=[" << content << "] think=[" << think
                      << "] tool_calls="
                      << (message.contains("tool_calls") ? message.at("tool_calls").size() : 0)
                      << '\n';
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

        const std::string payload =
            body.dump(-1, ' ', false, json::error_handler_t::replace);

        std::cerr << "[REQ] model=" << model.name
                  << " bytes=" << payload.size()
                  << " msgs=" << conversation.size()
                  << " tools=" << tools.size()
                  << " think=" << thinking << '\n';

        auto response = client->Post(
            "/api/chat",
            headers,
            payload,
            "application/json",
            receiver);

        if (!response)
            throw std::runtime_error("Falha ao conectar com o Ollama");

        if (response->status < 200 || response->status >= 300)
        {
            std::cerr << "\n========== ERRO OLLAMA ==========\n"
                      << "[Ollama] HTTP: " << response->status << '\n'
                      << "[Ollama] BODY: " << (rawHead.empty() ? response->body : rawHead) << '\n'
                      << "[Ollama] Origem: Ollama.cpp (chatStream)\n"
                      << "================================\n"
                      << std::flush;

            throw std::runtime_error(
                "Ollama retornou HTTP " + std::to_string(response->status));
        }

        processLine(buffer);
        buffer.clear();

        std::cout << "[Ollama] Geração finalizada."
                  << " Motivo: " << doneReason
                  << " | Truncada: " << truncated
                  << " | Caracteres: " << currentGeneration.size()
                  << '\n';

        if (!truncated)
            break;

        // Tool call cortada não pode ser continuada com texto.
        if (!toolCalls.empty())
            break;

        if (++continuations > kMaxContinuations)
        {
            std::cerr << "[Ollama] Limite de continuações atingido.\n";
            break;
        }

        if (currentGeneration.empty())
        {
            // O pensamento consumiu todo o num_predict: tenta sem thinking.
            if (thinking)
            {
                thinking = false;
                conversation.push_back({{"role", "user"},
                                        {"content", "Responda diretamente, sem raciocinar."}});
                continue;
            }

            std::cerr << "[Ollama] Truncado sem conteúdo: aumente num_ctx/num_predict.\n";
            break;
        }

        conversation.push_back({{"role", "assistant"}, {"content", currentGeneration}});
        conversation.push_back({{"role", "user"},
                                {"content", "Continue exatamente de onde parou, sem repetir o conteúdo anterior."}});
    }

    return {responseText, toolCalls, responseThink};
}