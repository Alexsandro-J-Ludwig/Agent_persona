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
#include "src/configs/models.h"
#include "src/ollama/ollama.h"
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

class InternalTools
{
public:
    using json = nlohmann::json;

    json getSchema() const;

    ToolResult executeModelTool(
        std::string_view name,
        const std::string &arguments);

    ToolResult executeSystemTool(
        std::string_view name,
        const std::string &arguments);

private:
    Ollama ollama;

    std::unique_ptr<httplib::Client> client =
        std::make_unique<httplib::Client>("http://127.0.0.1:5001");

    ToolResult execute(
        ToolCaller caller,
        std::string_view name,
        const std::string &arguments);

    bool isAuthorized(
        ToolCaller caller,
        std::string_view name) const;

    ToolResult decideThink(
        const std::string &arguments);

    ToolResult compressor(
        const std::string &arguments);

    // ToolResult audioPlayer(const std::string &response);
};

#endif
