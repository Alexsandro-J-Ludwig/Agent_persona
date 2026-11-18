#ifndef MEMORY_H
#define MEMORY_H

#include <sqlite3.h>
#include <cstdint>
#include <mutex>
#include <string>

class InternalTools; // forward declaration, sem include

struct FactResult
{
    int32_t factID = 0;
    bool success = false;
    std::string output;
    std::string error;
};

struct MemoryResult
{
    int32_t chatID = 0;
    bool success = false;
    std::string output;
    std::string error;
};

class Memory
{
public:
    static Memory &instance()
    {
        static Memory m;
        return m;
    }

    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
    ~Memory();

    bool initialize(InternalTools &t);
    void close();

    // ===== Facts =====
    FactResult rememberFact(const std::string &fact);
    FactResult forgetFact(int32_t factID);
    FactResult searchFact(const std::string &query);
    FactResult listFact(int32_t limit);

    // ===== Chats =====
    MemoryResult newChat();
    MemoryResult chatHistory(
        int32_t chatID = 0,
        const std::string &role = "",
        const std::string &message = "");
    MemoryResult loadChat(int32_t chatID, int32_t limit);
    MemoryResult listChats();
    MemoryResult deleteChat(int32_t chatID);
    MemoryResult saveSummary(int32_t chatID, const std::string &summary);
    MemoryResult setChatTitle(int32_t chatID, const std::string &title);

    // Gera o título via InternalTools e grava; nunca falha (usa fallback)
    MemoryResult generateChatTitle(int32_t chatID, const std::string &firstMessage);

private:
    Memory() = default;
    int32_t insertChat();

    InternalTools *tools = nullptr;
    sqlite3 *db = nullptr;
    std::mutex mtx;

    bool chatExists(int32_t chatID);
};

#endif