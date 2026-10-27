#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>

#include "core/CConversationStore.h"

namespace
{
std::filesystem::path make_temp_db()
{
    return std::filesystem::temp_directory_path()
        / ("fcpp-conversation-" + std::to_string(static_cast<long>(::getpid())) + ".sqlite");
}
}

TEST_CASE("conversation store persists session state and append-only events", "[conversation]")
{
    const auto path = make_temp_db();
    std::filesystem::remove(path);

    {
        fcpp::CConversationStore store;
        REQUIRE(store.open(path));
        REQUIRE(store.create_session("session-1", "project-1"));
        REQUIRE(store.update_context("session-1", R"({"goal":"inspect"})"));

        std::int64_t event_id = 0;
        REQUIRE(store.append_event("session-1", "user", R"({"text":"hello"})", &event_id));
        REQUIRE(event_id > 0);
        REQUIRE(store.append_event("session-1", "assistant", R"({"text":"hi"})"));

        const auto events = store.events("session-1");
        REQUIRE(events.size() == 2);
        REQUIRE(events[0].m_kind == "user");
        REQUIRE(events[0].m_payload == R"({"text":"hello"})");
        REQUIRE(events[1].m_kind == "assistant");
        const auto turns = fcpp::reconstruct_conversation_turns(events);
        REQUIRE(turns.size() == 1);
        REQUIRE(turns[0].m_user_text == "hello");
        REQUIRE(turns[0].m_assistant_text == "hi");
        REQUIRE(turns[0].m_last_event_id == events[1].m_id);

        const auto session = store.get_session("session-1");
        REQUIRE(session.has_value());
        REQUIRE(session->m_project_id == "project-1");
        REQUIRE(session->m_context_json == R"({"goal":"inspect"})");
        store.close();
    }

    {
        fcpp::CConversationStore store;
        REQUIRE(store.open(path));
        const auto events = store.events("session-1");
        REQUIRE(events.size() == 2);
        const auto turns = fcpp::reconstruct_conversation_turns(events);
        REQUIRE(turns.size() == 1);
        REQUIRE(turns[0].m_user_text == "hello");
        REQUIRE(turns[0].m_assistant_text == "hi");
        const auto restored_session = store.get_session("session-1");
        REQUIRE(restored_session.has_value());
        REQUIRE(restored_session->m_context_json == R"({"goal":"inspect"})");
        REQUIRE(store.close_session("session-1"));
        REQUIRE_FALSE(store.append_event("session-1", "user", "late"));
        REQUIRE(store.get_session("session-1")->m_closed);
    }

    std::filesystem::remove(path);
}

TEST_CASE("conversation store rejects missing and closed sessions", "[conversation]")
{
    const auto path = make_temp_db();
    std::filesystem::remove(path);

    fcpp::CConversationStore store;
    REQUIRE(store.open(path));
    REQUIRE_FALSE(store.append_event("missing", "user", "x"));
    REQUIRE_FALSE(store.create_session("", ""));
    REQUIRE(store.create_session("s", ""));
    REQUIRE(store.close_session("s"));
    REQUIRE_FALSE(store.update_context("s", "{}"));
    store.close();
    std::filesystem::remove(path);
}


TEST_CASE("conversation replay preserves unanswered user turns and skips malformed events", "[conversation]")
{
    const std::vector<fcpp::SConversationEvent> events{
        {1, "s", "user", R"({"text":"first"})", 1},
        {2, "s", "assistant", R"({"text":"answer"})", 2},
        {3, "s", "user", R"({"text":"pending"})", 3},
        {4, "s", "assistant", "not-json", 4}
    };

    const auto turns = fcpp::reconstruct_conversation_turns(events);
    REQUIRE(turns.size() == 2);
    REQUIRE(turns[0].m_user_text == "first");
    REQUIRE(turns[0].m_assistant_text == "answer");
    REQUIRE(turns[0].m_last_event_id == 2);
    REQUIRE(turns[1].m_user_text == "pending");
    REQUIRE(turns[1].m_assistant_text.empty());
    REQUIRE(turns[1].m_last_event_id == 3);
}

TEST_CASE("conversation session lifecycle mutations roll back with their events", "[conversation]")
{
    const auto path = make_temp_db();
    std::filesystem::remove(path);

    fcpp::CConversationStore store;
    REQUIRE(store.open(path));

    // An invalid event kind forces the second step to fail; the session insert must roll back.
    REQUIRE_FALSE(store.create_session_with_event("atomic-open", "project", "", "{}"));
    REQUIRE_FALSE(store.get_session("atomic-open").has_value());

    REQUIRE(store.create_session("atomic-context", "project"));
    REQUIRE(store.update_context("atomic-context", R"({"goal":"original"})"));
    REQUIRE_FALSE(store.update_context_with_event(
        "atomic-context", R"({"goal":"changed"})", "", R"({"goal":"changed"})"));
    const auto unchanged = store.get_session("atomic-context");
    REQUIRE(unchanged.has_value());
    REQUIRE(unchanged->m_context_json == R"({"goal":"original"})");
    REQUIRE(store.events("atomic-context").empty());

    REQUIRE_FALSE(store.close_session_with_event("atomic-context", "", "{}"));
    const auto still_open = store.get_session("atomic-context");
    REQUIRE(still_open.has_value());
    REQUIRE_FALSE(still_open->m_closed);
    REQUIRE(store.events("atomic-context").empty());

    REQUIRE(store.update_context_with_event(
        "atomic-context", R"({"goal":"committed"})",
        "session.context_updated", R"({"goal":"committed"})"));
    REQUIRE(store.close_session_with_event("atomic-context", "session.close", "{}"));

    const auto closed = store.get_session("atomic-context");
    REQUIRE(closed.has_value());
    REQUIRE(closed->m_closed);
    const auto events = store.events("atomic-context");
    REQUIRE(events.size() == 2);
    REQUIRE(events[0].m_kind == "session.context_updated");
    REQUIRE(events[1].m_kind == "session.close");

    store.close();
    std::filesystem::remove(path);
}

TEST_CASE("conversation state projection preserves provenance and only trusts typed events", "[conversation]")
{
    const std::vector<fcpp::SConversationEvent> events{
        {1, "s", "assistant", R"({"text":"The API is probably thread-safe"})", 10},
        {2, "s", "state.goal", R"({"id":"goal","text":"Make session updates atomic","status":"confirmed","source_ref":"event:2"})", 20},
        {3, "s", "state.fact", R"({"id":"sqlite","text":"SQLite uses a single writer","status":"verified","source_ref":"docs/sqlite"})", 30},
        {4, "s", "state.fact", R"({"id":"sqlite","text":"SQLite serializes writers","status":"verified","source_ref":"event:4"})", 40},
        {5, "s", "state.open_question", R"({"id":"q1","text":"Which model tokenizer is configured?"})", 50},
        {6, "s", "state.decision", "not-json", 60},
        {7, "s", "state.fact", R"({"id":"missing-text"})", 70}
    };

    const auto state = fcpp::reconstruct_conversation_state(events);
    REQUIRE(state.size() == 3);

    REQUIRE(state[0].m_kind == "state.goal");
    REQUIRE(state[0].m_id == "goal");
    REQUIRE(state[0].m_text == "Make session updates atomic");
    REQUIRE(state[0].m_status == "confirmed");
    REQUIRE(state[0].m_source_ref == "event:2");
    REQUIRE(state[0].m_source_event_id == 2);
    REQUIRE(state[0].m_updated_at == 20);

    REQUIRE(state[1].m_kind == "state.fact");
    REQUIRE(state[1].m_id == "sqlite");
    REQUIRE(state[1].m_text == "SQLite serializes writers");
    REQUIRE(state[1].m_source_ref == "event:4");
    REQUIRE(state[1].m_source_event_id == 4);

    REQUIRE(state[2].m_kind == "state.open_question");
    REQUIRE(state[2].m_status == "unspecified");
    REQUIRE(state[2].m_source_event_id == 5);
}

TEST_CASE("conversation shrink keeps the target question and rewinds the active projection", "[conversation][shrink]")
{
    const auto path = make_temp_db();
    std::filesystem::remove(path);

    fcpp::CConversationStore store;
    REQUIRE(store.open(path));
    REQUIRE(store.create_session("shrink-session", "project"));

    REQUIRE(store.append_event("shrink-session", "user", R"({"text":"first question"})"));
    REQUIRE(store.append_event("shrink-session", "assistant",
        R"({"text":"Choose a compiler","question":{"wait_for_user":true,"questions":[{"id":42,"text":"Choose a compiler","options":["GCC","Clang"],"and":false}]}})"));
    REQUIRE(store.append_event("shrink-session", "user",
        R"({"text":"GCC","answer_question_id":42,"selected_option":"GCC"})"));
    REQUIRE(store.append_event("shrink-session", "assistant", R"({"text":"GCC selected","ok":true})"));
    REQUIRE(store.append_event("shrink-session", "state.fact",
        R"({"id":"later-fact","text":"must be hidden after shrink"})"));
    REQUIRE(store.append_event("shrink-session", "user", R"({"text":"later question"})"));

    const auto before = store.events("shrink-session");
    REQUIRE(before.size() == 6);
    REQUIRE(store.shrink_question("shrink-session", 42));

    const auto raw_after = store.events("shrink-session");
    REQUIRE(raw_after.size() == 7);
    REQUIRE(raw_after.back().m_kind == "session.question_shrunk");

    const auto active = fcpp::active_conversation_events(raw_after);
    REQUIRE(active.size() == 2);
    REQUIRE(active[0].m_kind == "user");
    REQUIRE(active[1].m_kind == "assistant");
    REQUIRE(active[1].m_payload.find("Choose a compiler") != std::string::npos);

    const auto turns = fcpp::reconstruct_conversation_turns(raw_after);
    REQUIRE(turns.size() == 1);
    REQUIRE(turns[0].m_user_text == "first question");
    REQUIRE(turns[0].m_assistant_text == "Choose a compiler");

    const auto state = fcpp::reconstruct_conversation_state(raw_after);
    REQUIRE(state.empty());
    REQUIRE_FALSE(store.shrink_question("shrink-session", 42));

    REQUIRE(store.append_event("shrink-session", "user", R"({"text":"new question"})"));
    REQUIRE(store.append_event("shrink-session", "assistant", R"({"text":"new answer","ok":true})"));
    const auto active_after_new_turn = fcpp::active_conversation_events(store.events("shrink-session"));
    REQUIRE(active_after_new_turn.size() == 4);
    REQUIRE(fcpp::reconstruct_conversation_turns(store.events("shrink-session")).size() == 2);

    store.close();
    std::filesystem::remove(path);
}

TEST_CASE("conversation shrink rejects missing, closed, and non-question targets", "[conversation][shrink]")
{
    const auto path = make_temp_db();
    std::filesystem::remove(path);

    fcpp::CConversationStore store;
    REQUIRE(store.open(path));
    REQUIRE_FALSE(store.shrink_question("missing", 1));
    REQUIRE(store.create_session("s", "project"));
    REQUIRE_FALSE(store.shrink_question("s", 1));
    REQUIRE(store.append_event("s", "assistant", R"({"text":"not a question"})"));
    REQUIRE_FALSE(store.shrink_question("s", 1));
    REQUIRE(store.close_session("s"));
    REQUIRE_FALSE(store.shrink_question("s", 1));

    store.close();
    std::filesystem::remove(path);
}


TEST_CASE("conversation replay preserves cursor coordinates for follow-up context", "[conversation]")
{
    const std::vector<fcpp::SConversationEvent> events{
        {1, "s", "user",
         R"({"text":"Тут есть ошибки?","editor_file":"/project/src/a.cpp","editor_line":17,"editor_column":5})", 1},
        {2, "s", "assistant", R"({"text":"Обсуждаемый метод содержит проверку входных данных."})", 2},
        {3, "s", "user", R"({"text":"А что он делает?","editor_file":"","editor_line":0,"editor_column":0})", 3},
        {4, "s", "assistant", R"({"text":"Он проверяет входные данные."})", 4}
    };

    const auto turns = fcpp::reconstruct_conversation_turns(events);
    REQUIRE(turns.size() == 2);
    REQUIRE(turns[0].m_editor_file == "/project/src/a.cpp");
    REQUIRE(turns[0].m_editor_line == 17);
    REQUIRE(turns[0].m_editor_column == 5);
    REQUIRE(turns[1].m_editor_file.empty());
    REQUIRE(turns[1].m_editor_line == 0);
    REQUIRE(turns[1].m_editor_column == 0);
}
