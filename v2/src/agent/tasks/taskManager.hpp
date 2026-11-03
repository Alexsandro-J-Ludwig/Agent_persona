
#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "src/agent/internal_tools/internalTools.h"

#include <cstddef>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

enum class TaskStatus
{
    Pending,
    InProgress,
    Finished,
    Blocked
};

using OnToolResult = std::function<void(
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

struct ActivePlan
{
    std::string text;
    std::vector<std::string> steps;

    std::size_t currentStep = 0;
    int attempts = 0;
    int maxAttempts = 3;

    TaskStatus status = TaskStatus::Pending;

    std::string blockedReason;
};

class TaskManager
{
public:
    explicit TaskManager(InternalTools &tools)
        : tools(tools) {}

    // Execução das ferramentas
    void toolCalling(
        const nlohmann::json &toolCalls,
        const OnToolResult &onResult);

    void blockActivePlan(const std::string &reason);

    // Gerenciamento do plano
    bool hasActivePlan() const
    {
        return activePlan &&
               activePlan->status == TaskStatus::InProgress;
    }

    std::string currentInstruction() const;

    void reportStep(bool done);

    std::string finalReport() const;

private:
    InternalTools &tools;

    std::optional<ActivePlan> activePlan;

    void startPlanManagement(const std::string &planText);

    std::vector<std::string> parseSteps(
        const std::string &planText) const;

    int parseMaxAttempts(
        const std::string &planText) const;
};

#endif
