/**
 * @file CConversationStore.h
 * @brief Persistent storage for conversation sessions and immutable events.
 *
 * Conversation data is deliberately stored outside project index databases.
 */

#ifndef FCPP_AGENT_CCONVERSATIONSTORE_H
#define FCPP_AGENT_CCONVERSATIONSTORE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace fcpp
{

/** @brief Persisted session metadata and compact conversation state. */
struct SConversationSession
{
    std::string m_id;
    std::string m_project_id;
    std::string m_context_json;
    std::int64_t m_created_at = 0;
    std::int64_t m_updated_at = 0;
    bool m_closed = false;
};

/** @brief Immutable conversation event. Payload is opaque UTF-8/JSON text. */
struct SConversationEvent
{
    std::int64_t m_id = 0;
    std::string m_session_id;
    std::string m_kind;
    std::string m_payload;
    std::int64_t m_created_at = 0;
};

/** @brief Completed conversational turn reconstructed from immutable events. */
struct SConversationTurn
{
    std::int64_t m_last_event_id = 0;
    std::string m_user_text;
    std::string m_assistant_text;
};

/**
 * @brief SQLite store for session metadata and append-only conversation events.
 *
 * The store owns its SQLite connection and is not copyable. Call open() before
 * using any other method. Event history is never compacted or overwritten here.
 */
class CConversationStore
{
public:
    CConversationStore() = default;
    ~CConversationStore();

    CConversationStore(const CConversationStore&) = delete;
    CConversationStore& operator=(const CConversationStore&) = delete;

    bool open(const std::filesystem::path& database_path);
    void close();
    bool is_open() const;

    bool create_session(const std::string& session_id,
                        const std::string& project_id = {});
    bool close_session(const std::string& session_id);
    bool update_context(const std::string& session_id, const std::string& context_json);
    std::optional<SConversationSession> get_session(const std::string& session_id) const;

    bool append_event(const std::string& session_id,
                      const std::string& kind,
                      const std::string& payload,
                      std::int64_t* out_event_id = nullptr);
    std::vector<SConversationEvent> events(const std::string& session_id,
                                           std::int64_t after_event_id = 0,
                                           std::size_t limit = 1000) const;

private:
    sqlite3* m_db = nullptr;
};

} // namespace fcpp

#endif // FCPP_AGENT_CCONVERSATIONSTORE_H
