/**
 * @file CConversationStore.cpp
 * @brief SQLite persistence for conversation sessions.
 */

#include "core/CConversationStore.h"

#include <chrono>
#include <filesystem>
#include <system_error>

#include <sqlite3.h>

namespace fcpp
{
namespace
{

std::int64_t now_unix_seconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class Statement
{
public:
    Statement(sqlite3* db, const char* sql)
        : m_db(db)
    {
        m_ok = sqlite3_prepare_v2(db, sql, -1, &m_statement, nullptr) == SQLITE_OK;
    }

    ~Statement()
    {
        if (m_statement != nullptr)
        {
            sqlite3_finalize(m_statement);
        }
    }

    bool ok() const { return m_ok; }
    sqlite3_stmt* get() const { return m_statement; }

    bool bind_text(int index, const std::string& value)
    {
        return sqlite3_bind_text(m_statement, index, value.c_str(),
                                 static_cast<int>(value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
    }

    bool bind_int64(int index, std::int64_t value)
    {
        return sqlite3_bind_int64(m_statement, index, value) == SQLITE_OK;
    }

private:
    sqlite3* m_db = nullptr;
    sqlite3_stmt* m_statement = nullptr;
    bool m_ok = false;
};

std::string column_text(sqlite3_stmt* statement, int column)
{
    const unsigned char* value = sqlite3_column_text(statement, column);
    return value == nullptr ? std::string() : reinterpret_cast<const char*>(value);
}

} // namespace

CConversationStore::~CConversationStore()
{
    close();
}

bool CConversationStore::open(const std::filesystem::path& database_path)
{
    close();

    std::error_code error;
    if (!database_path.parent_path().empty())
    {
        std::filesystem::create_directories(database_path.parent_path(), error);
        if (error)
        {
            return false;
        }
    }

    if (sqlite3_open_v2(database_path.string().c_str(), &m_db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK)
    {
        close();
        return false;
    }

    constexpr const char* schema =
        "PRAGMA foreign_keys = ON;"
        "CREATE TABLE IF NOT EXISTS conversation_session ("
        "  session_id TEXT PRIMARY KEY NOT NULL,"
        "  project_id TEXT NOT NULL DEFAULT '',"
        "  context_json TEXT NOT NULL DEFAULT '{}',"
        "  created_at INTEGER NOT NULL,"
        "  updated_at INTEGER NOT NULL,"
        "  closed_at INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS conversation_event ("
        "  event_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  session_id TEXT NOT NULL REFERENCES conversation_session(session_id),"
        "  kind TEXT NOT NULL,"
        "  payload TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_conversation_event_session "
        "ON conversation_event(session_id, event_id);";

    char* error_message = nullptr;
    const int result = sqlite3_exec(m_db, schema, nullptr, nullptr, &error_message);
    if (error_message != nullptr)
    {
        sqlite3_free(error_message);
    }
    if (result != SQLITE_OK)
    {
        close();
        return false;
    }
    return true;
}

void CConversationStore::close()
{
    if (m_db != nullptr)
    {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
}

bool CConversationStore::is_open() const
{
    return m_db != nullptr;
}

bool CConversationStore::create_session(const std::string& session_id,
                                         const std::string& project_id)
{
    if (m_db == nullptr || session_id.empty())
    {
        return false;
    }

    const std::int64_t now = now_unix_seconds();
    Statement statement(m_db,
        "INSERT INTO conversation_session(session_id, project_id, context_json, created_at, updated_at) "
        "VALUES(?1, ?2, '{}', ?3, ?3)");
    return statement.ok()
        && statement.bind_text(1, session_id)
        && statement.bind_text(2, project_id)
        && statement.bind_int64(3, now)
        && sqlite3_step(statement.get()) == SQLITE_DONE;
}

bool CConversationStore::close_session(const std::string& session_id)
{
    if (m_db == nullptr || session_id.empty())
    {
        return false;
    }

    Statement statement(m_db,
        "UPDATE conversation_session SET closed_at=?1, updated_at=?1 "
        "WHERE session_id=?2 AND closed_at IS NULL");
    return statement.ok()
        && statement.bind_int64(1, now_unix_seconds())
        && statement.bind_text(2, session_id)
        && sqlite3_step(statement.get()) == SQLITE_DONE
        && sqlite3_changes(m_db) == 1;
}

bool CConversationStore::update_context(const std::string& session_id,
                                        const std::string& context_json)
{
    if (m_db == nullptr || session_id.empty())
    {
        return false;
    }

    Statement statement(m_db,
        "UPDATE conversation_session SET context_json=?1, updated_at=?2 "
        "WHERE session_id=?3 AND closed_at IS NULL");
    return statement.ok()
        && statement.bind_text(1, context_json)
        && statement.bind_int64(2, now_unix_seconds())
        && statement.bind_text(3, session_id)
        && sqlite3_step(statement.get()) == SQLITE_DONE
        && sqlite3_changes(m_db) == 1;
}

std::optional<SConversationSession> CConversationStore::get_session(
    const std::string& session_id) const
{
    if (m_db == nullptr || session_id.empty())
    {
        return std::nullopt;
    }

    Statement statement(m_db,
        "SELECT session_id, project_id, context_json, created_at, updated_at, closed_at "
        "FROM conversation_session WHERE session_id=?1");
    if (!statement.ok() || !statement.bind_text(1, session_id)
        || sqlite3_step(statement.get()) != SQLITE_ROW)
    {
        return std::nullopt;
    }

    SConversationSession session;
    session.m_id = column_text(statement.get(), 0);
    session.m_project_id = column_text(statement.get(), 1);
    session.m_context_json = column_text(statement.get(), 2);
    session.m_created_at = sqlite3_column_int64(statement.get(), 3);
    session.m_updated_at = sqlite3_column_int64(statement.get(), 4);
    session.m_closed = sqlite3_column_type(statement.get(), 5) != SQLITE_NULL;
    return session;
}

bool CConversationStore::append_event(const std::string& session_id,
                                      const std::string& kind,
                                      const std::string& payload,
                                      std::int64_t* out_event_id)
{
    if (m_db == nullptr || session_id.empty() || kind.empty())
    {
        return false;
    }

    Statement statement(m_db,
        "INSERT INTO conversation_event(session_id, kind, payload, created_at) "
        "SELECT ?1, ?2, ?3, ?4 WHERE EXISTS ("
        "SELECT 1 FROM conversation_session WHERE session_id=?1 AND closed_at IS NULL)");
    if (!statement.ok()
        || !statement.bind_text(1, session_id)
        || !statement.bind_text(2, kind)
        || !statement.bind_text(3, payload)
        || !statement.bind_int64(4, now_unix_seconds())
        || sqlite3_step(statement.get()) != SQLITE_DONE
        || sqlite3_changes(m_db) != 1)
    {
        return false;
    }

    if (out_event_id != nullptr)
    {
        *out_event_id = sqlite3_last_insert_rowid(m_db);
    }
    return true;
}

std::vector<SConversationEvent> CConversationStore::events(
    const std::string& session_id, std::int64_t after_event_id, std::size_t limit) const
{
    std::vector<SConversationEvent> result;
    if (m_db == nullptr || session_id.empty() || limit == 0)
    {
        return result;
    }

    Statement statement(m_db,
        "SELECT event_id, session_id, kind, payload, created_at "
        "FROM conversation_event WHERE session_id=?1 AND event_id>?2 "
        "ORDER BY event_id ASC LIMIT ?3");
    if (!statement.ok()
        || !statement.bind_text(1, session_id)
        || !statement.bind_int64(2, after_event_id)
        || sqlite3_bind_int64(statement.get(), 3, static_cast<sqlite3_int64>(limit)) != SQLITE_OK)
    {
        return result;
    }

    while (sqlite3_step(statement.get()) == SQLITE_ROW)
    {
        SConversationEvent event;
        event.m_id = sqlite3_column_int64(statement.get(), 0);
        event.m_session_id = column_text(statement.get(), 1);
        event.m_kind = column_text(statement.get(), 2);
        event.m_payload = column_text(statement.get(), 3);
        event.m_created_at = sqlite3_column_int64(statement.get(), 4);
        result.push_back(std::move(event));
    }
    return result;
}

} // namespace fcpp
