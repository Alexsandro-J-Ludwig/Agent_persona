#ifndef INTERNAL_TOOLS_H
#define INTERNAL_TOOLS_H

#include <array>
#include <filesystem>
#include <format>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <json.hpp>
#include <httplib.h>
#include <algorithm>
#include <cctype>
#include <fstream>

#include "src/configs/models.h"
#include "src/configs/agentconfig.h"
#include "src/ollama/ollama.h"
#include "src/agent/internal_tools/audio/ttsService.h"
#include "src/configs/searxng.h"

namespace fs = std::filesystem;

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
    Searxng searx_;

    std::string parametersOf(const std::string &toolName) const;

    static constexpr std::size_t MAX_BYTES = Config::Properties::max_bytes;

    ToolResult decideThink(const std::string &arguments);
    ToolResult compressor(const std::string &arguments);
    ToolResult audioPlayer(const std::string &response);
    ToolResult calculator(const std::string &arguments);
    ToolResult searchFiles(const std::string &arguments);
    ToolResult listDirectory(const std::string &arguments);
    ToolResult readCode(const std::string &arguments);
    ToolResult getUrls(const std::string &arguments);
    ToolResult getHTML(const std::string &arguments);

    inline static const std::array<ToolInfo, 9> TOOLS =
        {{{"decide_think", ToolCaller::System, &InternalTools::decideThink},
          {"audioPlayer", ToolCaller::System, &InternalTools::audioPlayer},
          {"compressor", ToolCaller::System, &InternalTools::compressor},
          {"calculator", ToolCaller::Model, &InternalTools::calculator},
          {"searchFiles", ToolCaller::Model, &InternalTools::searchFiles},
          {"listDirectory", ToolCaller::Model, &InternalTools::listDirectory},
          {"readCode", ToolCaller::Model, &InternalTools::readCode},
          {"getUrls", ToolCaller::Model, &InternalTools::getUrls},
          {"getHTML", ToolCaller::Model, &InternalTools::getHTML}}};
};

#endif