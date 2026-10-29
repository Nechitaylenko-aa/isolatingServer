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
#include <utility>
#include <vector>

struct sqlite3;

namespace fcpp
{

/**
 * @brief Persisted session metadata and compact conversation state.
 *
 * A session either exists or it does not. There is no "closed" state: closing a
 * conversation removes it together with its events.
 */
struct SConversationSession
{
    std::string m_id;
    std::string m_project_id;
    std::string m_context_json;
    std::int64_t m_created_at = 0;
    std::int64_t m_updated_at = 0;
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
    SConversationTurn() = default;

    SConversationTurn(std::int64_t last_event_id,
                      std::string user_text,
                      std::string assistant_text,
                      std::string editor_file = {},
                      unsigned editor_line = 0,
                      unsigned editor_column = 0)
        : m_last_event_id(last_event_id)
        , m_user_text(std::move(user_text))
        , m_assistant_text(std::move(assistant_text))
        , m_editor_file(std::move(editor_file))
        , m_editor_line(editor_line)
        , m_editor_column(editor_column)
    {
    }

    std::int64_t m_last_event_id = 0;
    std::string m_user_text;
    std::string m_assistant_text;
    std::string m_editor_file;
    unsigned m_editor_line = 0;
    unsigned m_editor_column = 0;
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
std::vector<SConversationEvent> active_conversation_events(
    const std::vector<SConversationEvent>& events);

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
     * @brief Необратимо привязать открытую сессию к проекту.
     *
     * Сессия без проекта (созданная до open) не должна становиться лазейкой для
     * смешивания историй: первое использование в открытом проекте фиксирует
     * project_id. Повторная привязка к другому проекту запрещена.
     * @param session_id Сессия.
     * @param project_id Проект.
     * @return True, если привязка установлена или уже совпадает.
     */
    bool bind_project(const std::string& session_id, const std::string& project_id);

    /**
     * @brief Удалить сессию и все её события одной транзакцией.
     *
     * Сессия либо есть, либо её нет. Закрытие — это удаление: события сессии
     * удаляются вместе с ней, чтобы не оставлять осиротевших записей.
     *
     * @param session_id Удаляемая сессия.
     * @return true если сессия и её события удалены.
     */
    bool delete_session(const std::string& session_id);

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

    /**
     * @brief Найти самую свежую открытую сессию проекта.
     *
     * Нужна для восстановления адреса диалога после перезапуска клиента: id
     * сессии генерирует только агент, поэтому клиент обязан спросить его заново.
     * Пустой project_id означает проект-независимую сессию (общие вопросы).
     * @param project_id Идентификатор проекта либо пустая строка.
     * @return Сессия или пустое значение, если открытых сессий нет.
     */
    std::optional<SConversationSession> latest_session_for_project(
        const std::string& project_id) const;

    bool append_event(const std::string& session_id,
                      const std::string& kind,
                      const std::string& payload,
                      std::int64_t* out_event_id = nullptr);
    std::vector<SConversationEvent> events(const std::string& session_id,
                                           std::int64_t after_event_id = 0,
                                           std::size_t limit = 1000) const;

    /**
     * @brief Логически обрезать активную историю до указанного вопроса включительно.
     *
     * Добавляет shrink marker в append-only журнал; raw audit events сохраняются,
     * а все conversation/state projections игнорируют удалённый хвост.
     */
    bool shrink_question(const std::string& session_id, std::uint16_t question_id);

private:
    sqlite3* m_db = nullptr;
};

} // namespace fcpp

#endif // FCPP_AGENT_CCONVERSATIONSTORE_H
