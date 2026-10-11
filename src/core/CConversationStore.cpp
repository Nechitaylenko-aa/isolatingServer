/**
 * @file CConversationStore.cpp
 * @brief SQLite persistence for conversation sessions.
 */

#include "core/CConversationStore.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

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
    sqlite3_stmt* m_statement = nullptr;
    bool m_ok = false;
};

std::string column_text(sqlite3_stmt* statement, int column)
{
    const unsigned char* value = sqlite3_column_text(statement, column);
    return value == nullptr ? std::string() : reinterpret_cast<const char*>(value);
}

/** @brief Roll back an unfinished multi-statement store operation. */
class Transaction
{
public:
    /**
     * @brief Start an immediate SQLite transaction.
     * @param db Open SQLite connection.
     */
    explicit Transaction(sqlite3* db)
        : m_db(db)
    {
        m_active = m_db != nullptr
            && sqlite3_exec(m_db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
    }

    /** @brief Roll back unless commit() completed successfully. */
    ~Transaction()
    {
        if (m_active)
        {
            sqlite3_exec(m_db, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    /**
     * @brief Check whether transaction startup succeeded.
     * @return True when a transaction is active.
     */
    bool ok() const
    {
        return m_active;
    }

    /**
     * @brief Commit the transaction.
     * @return True on successful commit.
     */
    bool commit()
    {
        if (!m_active)
        {
            return false;
        }
        if (sqlite3_exec(m_db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            return false;
        }
        m_active = false;
        return true;
    }

private:
    sqlite3* m_db = nullptr;
    bool m_active = false;
};

} // namespace

std::vector<SConversationEvent> active_conversation_events(
    const std::vector<SConversationEvent>& events)
{
    std::vector<SConversationEvent> active;
    for (const SConversationEvent& event : events)
    {
        if (event.m_kind != "session.question_shrunk")
        {
            active.push_back(event);
            continue;
        }

        // Shrink markers are audit events, not visible conversation items. They
        // rewind the active projection while retaining the append-only event log.
        try
        {
            const nlohmann::json payload = nlohmann::json::parse(event.m_payload);
            if (!payload.is_object() || !payload.contains("cutoff_event_id")
                    || !payload["cutoff_event_id"].is_number_integer())
            {
                continue;
            }
            const std::int64_t cutoff = payload["cutoff_event_id"].get<std::int64_t>();
            const auto target = std::find_if(active.begin(), active.end(),
                [cutoff](const SConversationEvent& item) { return item.m_id == cutoff; });
            if (cutoff <= 0 || target == active.end())
            {
                continue;
            }
            active.erase(std::remove_if(active.begin(), active.end(),
                [cutoff](const SConversationEvent& item) { return item.m_id > cutoff; }),
                active.end());
        }
        catch (const nlohmann::json::exception&)
        {
            // Ignore malformed shrink markers; never destroy historical events.
        }
    }
    return active;
}

std::vector<SConversationStateItem> reconstruct_conversation_state(
    const std::vector<SConversationEvent>& events)
{
    static const std::vector<std::string> supported_kinds{
        "state.goal",
        "state.constraint",
        "state.fact",
        "state.decision",
        "state.open_question",
        "state.completed_subtask"
    };

    std::vector<SConversationStateItem> state;
    const std::vector<SConversationEvent> active_events = active_conversation_events(events);
    for (const SConversationEvent& event : active_events)
    {
        if (std::find(supported_kinds.begin(), supported_kinds.end(), event.m_kind)
            == supported_kinds.end())
        {
            continue;
        }

        try
        {
            const nlohmann::json payload = nlohmann::json::parse(event.m_payload);
            if (!payload.is_object()
                || !payload.contains("id") || !payload["id"].is_string()
                || !payload.contains("text") || !payload["text"].is_string())
            {
                continue;
            }

            SConversationStateItem item;
            item.m_id = payload["id"].get<std::string>();
            item.m_text = payload["text"].get<std::string>();
            if (item.m_id.empty() || item.m_text.empty())
            {
                continue;
            }
            item.m_kind = event.m_kind;
            item.m_status = payload.value("status", std::string("unspecified"));
            item.m_source_ref = payload.value("source_ref", std::string());
            item.m_source_event_id = event.m_id;
            item.m_updated_at = event.m_created_at;

            const auto existing = std::find_if(
                state.begin(), state.end(),
                [&item](const SConversationStateItem& current)
                {
                    return current.m_id == item.m_id && current.m_kind == item.m_kind;
                });
            if (existing == state.end())
            {
                state.push_back(std::move(item));
            }
            else
            {
                *existing = std::move(item);
            }
        }
        catch (const nlohmann::json::exception&)
        {
            // Malformed state events are ignored; the append-only log remains intact.
        }
    }
    return state;
}

std::vector<SConversationTurn> reconstruct_conversation_turns(
    const std::vector<SConversationEvent>& events)
{
    const std::vector<SConversationEvent> active_events = active_conversation_events(events);
    std::vector<SConversationTurn> turns;
    std::string pending_user;
    std::int64_t pending_user_event_id = 0;

    for (const SConversationEvent& event : active_events)
    {
        try
        {
            const nlohmann::json payload = nlohmann::json::parse(event.m_payload);
            if (event.m_kind == "user" && payload.is_object()
                && payload.contains("text") && payload["text"].is_string())
            {
                if (!pending_user.empty())
                {
                    turns.push_back(SConversationTurn{pending_user_event_id, pending_user, {}});
                }
                pending_user = payload["text"].get<std::string>();
                pending_user_event_id = event.m_id;
            }
            else if (event.m_kind == "assistant" && payload.is_object()
                     && payload.contains("text") && payload["text"].is_string()
                     && !pending_user.empty())
            {
                turns.push_back(SConversationTurn{
                    event.m_id, pending_user, payload["text"].get<std::string>()});
                pending_user.clear();
                pending_user_event_id = 0;
            }
        }
        catch (const nlohmann::json::exception&)
        {
            // Keep the append-only event untouched; skip malformed legacy payloads in replay.
        }
    }

    if (!pending_user.empty())
    {
        turns.push_back(SConversationTurn{pending_user_event_id, pending_user, {}});
    }
    return turns;
}

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

bool CConversationStore::create_session_with_event(
    const std::string& session_id,
    const std::string& project_id,
    const std::string& event_kind,
    const std::string& event_payload)
{
    if (m_db == nullptr)
    {
        return false;
    }

    Transaction transaction(m_db);
    if (!transaction.ok()
        || !create_session(session_id, project_id)
        || !append_event(session_id, event_kind, event_payload))
    {
        return false;
    }
    return transaction.commit();
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

bool CConversationStore::close_session_with_event(
    const std::string& session_id,
    const std::string& event_kind,
    const std::string& event_payload)
{
    if (m_db == nullptr)
    {
        return false;
    }

    Transaction transaction(m_db);
    if (!transaction.ok()
        || !append_event(session_id, event_kind, event_payload)
        || !close_session(session_id))
    {
        return false;
    }
    return transaction.commit();
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

bool CConversationStore::update_context_with_event(
    const std::string& session_id,
    const std::string& context_json,
    const std::string& event_kind,
    const std::string& event_payload)
{
    if (m_db == nullptr)
    {
        return false;
    }

    Transaction transaction(m_db);
    if (!transaction.ok()
        || !update_context(session_id, context_json)
        || !append_event(session_id, event_kind, event_payload))
    {
        return false;
    }
    return transaction.commit();
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

bool CConversationStore::shrink_question(const std::string& session_id, std::uint16_t question_id)
{
    if (m_db == nullptr || session_id.empty() || question_id == 0)
    {
        return false;
    }

    Transaction transaction(m_db);
    if (!transaction.ok())
    {
        return false;
    }

    std::vector<SConversationEvent> all_events;
    std::int64_t after_event_id = 0;
    constexpr std::size_t k_page_size = 1000;
    while (true)
    {
        std::vector<SConversationEvent> page = events(session_id, after_event_id, k_page_size);
        if (page.empty())
        {
            break;
        }
        after_event_id = page.back().m_id;
        all_events.insert(all_events.end(), page.begin(), page.end());
        if (page.size() < k_page_size)
        {
            break;
        }
    }

    const std::vector<SConversationEvent> active_events = active_conversation_events(all_events);
    std::int64_t cutoff_event_id = 0;
    for (const SConversationEvent& event : active_events)
    {
        if (event.m_kind != "assistant")
        {
            continue;
        }
        try
        {
            const nlohmann::json payload = nlohmann::json::parse(event.m_payload);
            if (!payload.is_object() || !payload.contains("question")
                    || !payload["question"].is_object())
            {
                continue;
            }
            const nlohmann::json& question = payload["question"];
            if (!question.contains("questions") || !question["questions"].is_array())
            {
                continue;
            }
            for (const nlohmann::json& item : question["questions"])
            {
                if (item.is_object() && item.contains("id") && item["id"].is_number_integer()
                        && item["id"].get<std::int64_t>() == question_id)
                {
                    cutoff_event_id = event.m_id;
                    break;
                }
            }
        }
        catch (const nlohmann::json::exception&)
        {
            // Skip malformed historical assistant payloads.
        }
    }

    if (cutoff_event_id == 0)
    {
        return false;
    }
    if (!active_events.empty() && active_events.back().m_id == cutoff_event_id)
    {
        // Idempotent shrink: the requested question is already the active tail.
        return false;
    }

    const nlohmann::json marker = {
        {"question_id", question_id},
        {"cutoff_event_id", cutoff_event_id}
    };
    if (!append_event(session_id, "session.question_shrunk", marker.dump()))
    {
        return false;
    }
    return transaction.commit();
}

} // namespace fcpp
