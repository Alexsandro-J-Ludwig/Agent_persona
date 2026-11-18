#include "memory.h"
#include "definitions.h"
#include "src/agent/internal_tools/internalTools.h"
#include <iostream>

bool Memory::initialize(InternalTools &t)
{
    tools = &t;

    if (db)
        return true;

    const auto path = Definitions::ensure_workspace() / "memory.db";
    const auto u8 = path.u8string();

    if (sqlite3_open(reinterpret_cast<const char *>(u8.c_str()), &db) != SQLITE_OK)
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
    title TEXT,
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

    // Migração: bancos criados antes não têm a coluna title.
    // Se já existir, o SQLite devolve erro e ele é ignorado.
    sqlite3_exec(db, "ALTER TABLE chats ADD COLUMN title TEXT;", nullptr, nullptr, nullptr);

    return true;
}

// Helper interno: sem lock, chamado por quem já segura o mutex.
int32_t Memory::insertChat()
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO chats DEFAULT VALUES;", -1, &stmt, nullptr) != SQLITE_OK)
        return -1;

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE ? static_cast<int32_t>(sqlite3_last_insert_rowid(db)) : -1;
}

MemoryResult Memory::newChat()
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    const int32_t id = insertChat();
    if (id < 0)
        return {0, false, "", sqlite3_errmsg(db)};

    return {id, true, "Chat criado", ""};
}

bool Memory::chatExists(int32_t chatID)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM chats WHERE id = ? LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, chatID);
    const bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists;
}

MemoryResult Memory::chatHistory(int32_t chatID, const std::string &role, const std::string &message)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    if (!chatExists(chatID))
    {
        chatID = insertChat();
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

// UPDATE genérico de uma coluna de texto do chat
static bool updateChatText(sqlite3 *db, const char *sql, int32_t chatID, const std::string &text)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, text.c_str(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, chatID);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

MemoryResult Memory::setChatTitle(int32_t chatID, const std::string &title)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db || !chatExists(chatID))
        return {chatID, false, "", "Chat não encontrado"};

    if (!updateChatText(db, "UPDATE chats SET title = ? WHERE id = ?;", chatID, title))
        return {chatID, false, "", sqlite3_errmsg(db)};

    return {chatID, true, "Título salvo", ""};
}

MemoryResult Memory::saveSummary(int32_t chatID, const std::string &summary)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db || !chatExists(chatID))
        return {chatID, false, "", "Chat não encontrado"};

    if (!updateChatText(db, "UPDATE chats SET summary = ? WHERE id = ?;", chatID, summary))
        return {chatID, false, "", sqlite3_errmsg(db)};

    return {chatID, true, "Resumo salvo", ""};
}

MemoryResult Memory::generateChatTitle(int32_t chatID, const std::string &firstMessage)
{
    std::string title = "New chat";

    if (tools && !firstMessage.empty())
    {
        const auto r = tools->execute(ToolCaller::System, "generate_title", firstMessage).get();
        if (!r.output.empty())
            title = r.output;
    }

    return setChatTitle(chatID, title);
}

static std::string columnText(sqlite3_stmt *stmt, int col)
{
    const auto *t = sqlite3_column_text(stmt, col);
    return t ? reinterpret_cast<const char *>(t) : "";
}

FactResult Memory::rememberFact(const std::string &fact)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO facts (content) VALUES (?);", -1, &stmt, nullptr) != SQLITE_OK)
        return {0, false, "", sqlite3_errmsg(db)};

    sqlite3_bind_text(stmt, 1, fact.c_str(), static_cast<int>(fact.size()), SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return {0, false, "", sqlite3_errmsg(db)};

    const auto id = static_cast<int32_t>(sqlite3_last_insert_rowid(db));
    return {id, true, "Fato salvo", ""};
}

FactResult Memory::forgetFact(int32_t factID)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {factID, false, "", "Banco não inicializado"};

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM facts WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK)
        return {factID, false, "", sqlite3_errmsg(db)};

    sqlite3_bind_int(stmt, 1, factID);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return {factID, false, "", sqlite3_errmsg(db)};
    if (sqlite3_changes(db) == 0)
        return {factID, false, "", "Fato não encontrado"};

    return {factID, true, "Fato removido", ""};
}

FactResult Memory::searchFact(const std::string &query)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT id, content FROM facts WHERE content LIKE ? ORDER BY id DESC LIMIT 20;",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return {0, false, "", sqlite3_errmsg(db)};

    const std::string pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), static_cast<int>(pattern.size()), SQLITE_TRANSIENT);

    std::string out;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        out += "[" + std::to_string(sqlite3_column_int(stmt, 0)) + "] " + columnText(stmt, 1) + "\n";
    sqlite3_finalize(stmt);

    return {0, true, out.empty() ? "Nenhum fato encontrado" : out, ""};
}

FactResult Memory::listFact(int32_t limit)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT id, content FROM facts ORDER BY id DESC LIMIT ?;",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return {0, false, "", sqlite3_errmsg(db)};

    sqlite3_bind_int(stmt, 1, limit > 0 ? limit : 20);

    std::string out;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        out += "[" + std::to_string(sqlite3_column_int(stmt, 0)) + "] " + columnText(stmt, 1) + "\n";
    sqlite3_finalize(stmt);

    return {0, true, out.empty() ? "Nenhum fato salvo" : out, ""};
}

MemoryResult Memory::loadChat(int32_t chatID, int32_t limit)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db || !chatExists(chatID))
        return {chatID, false, "", "Chat não encontrado"};

    // Últimas N mensagens, devolvidas em ordem cronológica.
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT role, content FROM ("
                           "  SELECT id, role, content FROM messages"
                           "  WHERE chat_id = ? ORDER BY id DESC LIMIT ?"
                           ") ORDER BY id ASC;",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return {chatID, false, "", sqlite3_errmsg(db)};

    sqlite3_bind_int(stmt, 1, chatID);
    sqlite3_bind_int(stmt, 2, limit > 0 ? limit : 50);

    std::string out;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        out += columnText(stmt, 0) + ": " + columnText(stmt, 1) + "\n";
    sqlite3_finalize(stmt);

    return {chatID, true, out, ""};
}

MemoryResult Memory::listChats()
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db)
        return {0, false, "", "Banco não inicializado"};

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT id, COALESCE(title, 'New chat'), created_at FROM chats ORDER BY id DESC;",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return {0, false, "", sqlite3_errmsg(db)};

    std::string out;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        out += "[" + std::to_string(sqlite3_column_int(stmt, 0)) + "] " +
               columnText(stmt, 1) + " (" + columnText(stmt, 2) + ")\n";
    sqlite3_finalize(stmt);

    return {0, true, out.empty() ? "Nenhum chat" : out, ""};
}

MemoryResult Memory::deleteChat(int32_t chatID)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (!db || !chatExists(chatID))
        return {chatID, false, "", "Chat não encontrado"};

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM chats WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK)
        return {chatID, false, "", sqlite3_errmsg(db)};

    sqlite3_bind_int(stmt, 1, chatID);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return {chatID, false, "", sqlite3_errmsg(db)};

    return {chatID, true, "Chat removido", ""};
}

void Memory::close()
{
    std::lock_guard<std::mutex> lock(mtx);
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
}

Memory::~Memory()
{
    close();
}