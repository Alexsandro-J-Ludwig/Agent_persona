#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "src/agent/internal_tools/internalTools.h"

enum class TaskStatus
{
    Pending,
    InProgress,
    Finished,
    Blocked
};

using OnToolResult =
    std::function<void(
        const std::string &name,
        const ToolResult &result)>;

struct TaskResult
{
    bool success = false;
    std::string output;
    std::string error;
};

struct PendingTool
{
    std::string name;
    std::future<ToolResult> future;
};

class TaskManager
{
public:
    explicit TaskManager(InternalTools &tools) : tools(tools) {}

    void toolCalling(const nlohmann::json &toolCalls,
                     const OnToolResult &onResult);

private:
    InternalTools &tools;
};

#endif