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

#include "src/configs/models.h"
#include "src/ollama/ollama.h"
#include "src/agent/internal_tools/audio/ttsService.h"

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
    InternalTools() : InternalTools(std::filesystem::current_path()) {}
    explicit InternalTools(const std::filesystem::path &root);

    using json = nlohmann::json;

    json getSchema() const;

    std::future<ToolResult> execute(
        ToolCaller caller,
        std::string_view name,
        const std::string &arguments);

private:
    Ollama ollama;
    TTSService tts;

    std::filesystem::path rootDirectory;
    std::filesystem::path currentDirectory;
    std::mutex directoryMutex;

    static bool isInside(const std::filesystem::path &root,
                         const std::filesystem::path &candidate);
                         
    std::filesystem::path getCurrentDirectory();
    std::optional<std::filesystem::path> resolveInsideRoot(
        const std::string &path,
        const std::filesystem::path &base) const;

    ToolResult decideThink(const std::string &arguments);
    ToolResult compressor(const std::string &arguments);
    ToolResult audioPlayer(const std::string &response);
    ToolResult calculator(const std::string &arguments);
    ToolResult searchFiles(const std::string &arguments);
    ToolResult listDirectory(const std::string &arguments);
    ToolResult enterDirectory(const std::string &arguments);
    ToolResult exitDirectory(const std::string &arguments);
    ToolResult plan(const std::string &arguments);
    // ToolResult listTool(const std::string &arguments);

    inline static const std::array<ToolInfo, 9> TOOLS =
        {{{"decide_think", ToolCaller::System, &InternalTools::decideThink},
          {"audioPlayer", ToolCaller::System, &InternalTools::audioPlayer},
          {"compressor", ToolCaller::System, &InternalTools::compressor},
          {"calculator", ToolCaller::Model, &InternalTools::calculator},
          {"searchFiles", ToolCaller::Model, &InternalTools::searchFiles},
          {"listDirectory", ToolCaller::Model, &InternalTools::listDirectory},
          {"enterDirectory", ToolCaller::Model, &InternalTools::enterDirectory},
          {"exitDirectory", ToolCaller::Model, &InternalTools::exitDirectory}.
          {"plan", ToolCaller::Model, &InternalTools::plan}
        }};
        //   {"listTool", ToolCaller::Model, &InternalTools::listTool}
};

#endif