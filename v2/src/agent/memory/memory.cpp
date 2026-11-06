#include "memory.h"
#include "definitions.h"
#include <iostream>

bool Memory::inicialize()
{
    if (db)
        return true; // já aberto

    const auto path = Definitions::ensure_workspace() / "memory.db";
    const auto u8 = path.u8string();

    int rc = sqlite3_open(reinterpret_cast<const char *>(u8.c_str()), &db);
    if (rc != SQLITE_OK)
    {
        std::cerr << "[Memory] Erro ao abrir banco: " << sqlite3_errmsg(db) << '\n';
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    const char *schema = R"(
CREATE TABLE IF NOT EXISTS facts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    content TEXT NOT NULL,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS chats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    summary TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chat_id INTEGER NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);
)";

    char *err = nullptr;
    if (sqlite3_exec(db, schema, nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::cerr << "[Memory] Erro no schema: " << err << '\n';
        sqlite3_free(err);
        return false;
    }

    return true;
}

int32_t Memory::createChat()
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO chats DEFAULT VALUES;", -1, &stmt, nullptr) != SQLITE_OK)
        return -1;

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE ? static_cast<int32_t>(sqlite3_last_insert_rowid(db)) : -1;
}

// ======================================
// Cria um novo chat se não houver ID
// ======================================
int32_t Memory::newChat()
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO chats DEFAULT VALUES;", -1, &stmt, nullptr) != SQLITE_OK)
        return {0, false, "", sqlite3_errmsg(db)};

    const int rc = sqlite3_step(stmt);
    sqlite3_finaliz(stmt);

    if (rc != SQLITE_DONE)
        return {
            0,
            false,
            "",
            sqlite3_errmsg(db)};

    const auto chatID = static_cast<int32_t>(sqlite3_last_insert_rowid(db));
    return {chatID, true, "Chat criado", ""};
}

bool Memory::chatExists(int32_t chatID)
{
    // Sem lock aqui: é um helper interno, chamado por métodos que já seguram o mutex.
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM chats WHERE id = ? LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, chatID);

    const bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists;
}

// ======================================
// Insere mensagens no chat com ID
// ======================================
MemoryResult Memory::chatHistory(int32_t chatID, const std::string &role, const std::string &message)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    if (!chatExists(chatID))
    {
        chatID = createChat();
        if (chatID < 0)
            return {0, false, "", sqlite3_errmsg(db)};
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO messages (chat_id, role, content) VALUES (?, ?, ?);",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return {chatID, false, "", sqlite3_errmsg(db)};

    sqlite3_bind_int(stmt, 1, chatID);
    sqlite3_bind_text(stmt, 2, role.c_str(), static_cast<int>(role.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, message.c_str(), static_cast<int>(message.size()), SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return {chatID, false, "", sqlite3_errmsg(db)};

    return {chatID, true, "Mensagem salva", ""};
}
Memory::~Memory()
{
    if (db)
        sqlite3_close(db);
}
