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
