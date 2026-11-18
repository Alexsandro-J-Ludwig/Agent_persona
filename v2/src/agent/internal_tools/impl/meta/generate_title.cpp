#include "internalTools.h"
#include <format>

ToolResult InternalTools::generate_title(const std::string &arguments)
{
    const std::string fallback = "New chat";

    try
    {
        const std::string prompt = std::format(R"(You are a tool that creates a chat title from the user's message.

Rules:
- Between 4 and 10 words.
- One line only, no quotes, no final punctuation.
- Use the same language as the user's message.
- Output only the title, nothing else.

User message:
{})",
                                               arguments);

        auto response = ollama.generate(getModel(ModelRole::Agent), prompt, false);
        std::string title = response.at("response").get<std::string>();

        title = title.substr(0, title.find('\n'));

        if (title.empty())
            return {false, fallback, "Titulo vazio"};

        return {true, title, ""};
    }
    catch (const std::exception &e)
    {
        return {false, fallback, e.what()};
    }
}