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
 * @brief One current item in the deterministic conversation-state projection.
 *
 * State items are derived only from explicitly typed state.* events. Ordinary
 * assistant text is never promoted to a fact automatically.
 */
struct SConversationStateItem
{
    std::string m_id;
    std::string m_kind;
    std::string m_text;
    std::string m_status;
    std::string m_source_ref;
    std::int64_t m_source_event_id = 0;
    std::int64_t m_updated_at = 0;
};

/**
 * @brief Rebuild current conversation state from append-only state events.
 *
 * Supported event kinds are state.goal, state.constraint, state.fact,
 * state.decision, state.open_question, and state.completed_subtask. Payloads
 * must be JSON objects with non-empty string fields id and text. Optional
 * status and source_ref fields are copied verbatim. For a repeated (kind, id),
 * the latest valid event wins while its event id remains explicit provenance.
 * Malformed or unrelated events are ignored without mutating the event log.
 */
std::vector<SConversationStateItem> reconstruct_conversation_state(
    const std::vector<SConversationEvent>& events);

/** @brief Reconstruct user/assistant turns from stored JSON events. Malformed payloads are skipped. */
std::vector<SConversationTurn> reconstruct_conversation_turns(
    const std::vector<SConversationEvent>& events);

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

    /**
     * @brief Create a session and append its opening event atomically.
     * @param session_id Unique session identifier.
     * @param project_id Project bound to the session, if any.
     * @param event_kind Opening event kind.
     * @param event_payload Opening event payload.
     * @return True when both rows are committed.
     */
    bool create_session_with_event(const std::string& session_id,
                                   const std::string& project_id,
                                   const std::string& event_kind,
                                   const std::string& event_payload);

    /**
     * @brief Create a session without an initial event.
     * @param session_id Unique session identifier.
     * @param project_id Project bound to the session, if any.
     * @return True when the session row is created.
     */
    bool create_session(const std::string& session_id,
                        const std::string& project_id = {});

    /**
     * @brief Append a closing event and close the session atomically.
     * @param session_id Session to close.
     * @param event_kind Closing event kind.
     * @param event_payload Closing event payload.
     * @return True when the event and closed state are committed.
     */
    bool close_session_with_event(const std::string& session_id,
                                  const std::string& event_kind,
                                  const std::string& event_payload);

    /**
     * @brief Mark a session closed without appending an event.
     * @param session_id Session to close.
     * @return True when the session transitions from open to closed.
     */
    bool close_session(const std::string& session_id);

    /**
     * @brief Persist context and its audit event atomically.
     * @param session_id Session to update.
     * @param context_json Validated JSON context.
     * @param event_kind Context update event kind.
     * @param event_payload Context update event payload.
     * @return True when both state and event are committed.
     */
    bool update_context_with_event(const std::string& session_id,
                                   const std::string& context_json,
                                   const std::string& event_kind,
                                   const std::string& event_payload);

    /**
     * @brief Replace context for an open session without an event.
     * @param session_id Session to update.
     * @param context_json Context JSON.
     * @return True when the context is updated.
     */
    bool update_context(const std::string& session_id, const std::string& context_json);

    /**
     * @brief Retrieve persisted session metadata.
     * @param session_id Session identifier.
     * @return Session metadata, or empty when not found.
     */
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
