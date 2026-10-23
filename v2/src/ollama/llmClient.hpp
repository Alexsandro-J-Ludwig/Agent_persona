#ifndef LLMCLIENT_H
#define LLMCLIENT_H

struct ChatResult
{
    std::string response;
    nlohmann::json toolCalls = nlohmann::json::array();
};

class Motor
{
public:
    Motor();
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
        bool thinking = false,
        const json &tools = json::array());

private:
    std::unique_ptr<httplib::Client> client;
};

#endif