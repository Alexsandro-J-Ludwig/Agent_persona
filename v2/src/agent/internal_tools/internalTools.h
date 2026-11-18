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

using json = nlohmann::json;

class InternalTools
{
public:
    json getSchema() const;

    std::future<ToolResult> execute(
        ToolCaller caller,
        std::string_view name,
        const std::string &arguments);

    void disableGroup(const std::string &group);
    void setToolEnabled(const std::string &tool, bool enabled);

private:
    Ollama ollama;
    TTSService tts;
    Searxng searx_;

    mutable std::mutex groupsMtx_;
    std::unordered_set<std::string> disabledTools_;
    std::unordered_set<std::string> activeGroups_;

    std::mutex directoryMutex;
    std::filesystem::path currentDirectory;
    std::filesystem::path rootDirectory;

    bool isToolActive(std::string_view tool) const;

    std::string
    parametersOf(const std::string &toolName) const;

    static constexpr std::size_t MAX_BYTES = Config::Properties::max_bytes;

    ToolResult decideThink(const std::string &arguments);
    ToolResult compressor(const std::string &arguments);
    ToolResult audioPlayer(const std::string &response);
    ToolResult stopAudio(const std::string &arguments);
    ToolResult calculator(const std::string &arguments);
    ToolResult searchFiles(const std::string &arguments);
    ToolResult listDirectory(const std::string &arguments);
    ToolResult readCode(const std::string &arguments);
    ToolResult getUrls(const std::string &arguments);
    ToolResult getHTML(const std::string &arguments);
    ToolResult makeDir(const std::string &arguments);
    ToolResult writeFile(const std::string &arguments);
    ToolResult listToolGroup(const std::string &arguments);
    ToolResult checkSyntax(const std::string &arguments);
    ToolResult runCode(const std::string &arguments);
    ToolResult generate_title(const std::string &arguments);

    inline static const std::array<ToolInfo, 16> TOOLS =
        {{{"decide_think", ToolCaller::System, &InternalTools::decideThink},
          {"audioPlayer", ToolCaller::System, &InternalTools::audioPlayer},
          {"stopAudio", ToolCaller::System, &InternalTools::stopAudio},
          {"compressor", ToolCaller::System, &InternalTools::compressor},
          {"generate_title", ToolCaller::System, &InternalTools::generate_title},
          {"calculator", ToolCaller::Model, &InternalTools::calculator},
          {"searchFiles", ToolCaller::Model, &InternalTools::searchFiles},
          {"listDirectory", ToolCaller::Model, &InternalTools::listDirectory},
          {"readCode", ToolCaller::Model, &InternalTools::readCode},
          {"getUrls", ToolCaller::Model, &InternalTools::getUrls},
          {"getHTML", ToolCaller::Model, &InternalTools::getHTML},
          {"makeDir", ToolCaller::Model, &InternalTools::makeDir},
          {"writeFile", ToolCaller::Model, &InternalTools::writeFile},
          {"listToolGroup", ToolCaller::Model, &InternalTools::listToolGroup},
          {"checkSyntax", ToolCaller::Model, &InternalTools::checkSyntax},
          {"runCode", ToolCaller::Model, &InternalTools::runCode}}};
};

#endif