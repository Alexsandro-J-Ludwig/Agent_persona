#ifndef INTERNAL_TOOLS_H
#define INTERNAL_TOOLS_H

#include <string>
#include <string_view>
#include <vector>
#include <json.hpp>
#include <string>
#include <string_view>
#include <unordered_set>
#include <format>
#include <future>
#include "src/configs/models.h"
#include "src/ollama/ollama.h"
#include "src/agent/internal_tools/audio/ttsService.h"

#include <httplib.h>

enum class ToolCaller
{
    Model,
    System
};

struct ToolResult
{
    bool success = false;
    std::string output;
    std::string error;
};

class InternalTools;

struct ToolInfo
{
    std::string name;
    ToolCaller caller;
    ToolResult (InternalTools::*handler)(const std::string &);
};

class InternalTools
{
public:
    using json = nlohmann::json;

    json getSchema() const;

    std::future<ToolResult> execute(
        ToolCaller caller,
        std::string_view name,
        const std::string &arguments);

private:
    Ollama ollama;
    TTSService tts;

    ToolResult decideThink(const std::string &arguments);
    ToolResult compressor(const std::string &arguments);
    ToolResult audioPlayer(const std::string &response);
    ToolResult calculator(const std::string &arguments);
    ToolResult searchFiles(const std::string &arguments);

    inline static const std::array<ToolInfo, 5> TOOLS = {{
        {"decide_think", ToolCaller::System, &InternalTools::decideThink},
        {"audioPlayer", ToolCaller::System, &InternalTools::audioPlayer},
        {"compressor", ToolCaller::System, &InternalTools::compressor},
        {"calculator", ToolCaller::Model, &InternalTools::calculator},
        {"searchFiles", ToolCaller::Model, &InternalTools::searchFiles},
    }};
};

#endif
