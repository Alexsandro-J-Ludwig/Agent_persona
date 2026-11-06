#ifndef MEMORY_H
#define MEMORY_H

#include <sqlite3.h>
#include <string>
#include <cstdint>
#include <mutex

struct FactResult
{
    int32_t factID;
    bool success;
    std::string output;
    std::string error;
};

struct MemoryResult
{
    int32_t chatID;
    bool success;
    std::string output;
    std::string error;
};

class Memory
{
private:
    Memory() = default;
    sqlite3 *db = nullptr;
    std::mutex mtx;

    bool chatExists(int32_t chatID);

public:
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
    ~Memory();

    static Memory &memory()
    {
        static Memory instance;
        return instance;
    }

    bool inicialize();

    int32_t createChat();

    // ===== Fact Managment =====
    FactResult rememberFact(const std::string &fact); // Não implementado
    FactResult forgetFact(const int32_t factID);      // Não implementado
    FactResult searchFact(const std::string &fact);   // Não implementado
    FactResult listFact(const int32_t limit);         // Não implementado

    // ===== Memory managment =====
    MemoryResult newChat();
    MemoryResult chatHistory(
        const int32_t chatID = 0,
        const std::string &role,
        const std::string &message);                                     // Não implementado
    MemoryResult loadChat(const int32_t chatID, const int32_t limit);    // Não implementado
    MemoryResult listChats();                                            // Não implementado
    MemoryResult deleteChat(const int32_t chatID);                       // Não implementado
    MemoryResult saveSummary(const int32_t chatID, std::string summary); // Não implementado

    void close(); // Não implementado
};

#endif