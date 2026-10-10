#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

#include "core/CContextManager.h"

namespace
{
fcpp::SContextGroup group(std::string id, std::string text, std::size_t order,
                          int priority, bool mandatory = false)
{
    fcpp::SContextGroup value;
    value.m_id = std::move(id);
    value.m_order = order;
    value.m_priority = priority;
    value.m_mandatory = mandatory;
    value.m_messages.push_back({fcpp::EModelRole::User, std::move(text), {}, {}});
    return value;
}
}

TEST_CASE("conversation compaction summarizes old turns and keeps recent turns intact", "[context]")
{
    const std::vector<fcpp::SConversationTurn> history{
        {2, "old question one", "old answer one"},
        {4, "old question two", "old answer two"},
        {6, "recent question", "recent answer"}
    };

    const auto result = fcpp::compact_conversation_history(history, 1, 200);

    REQUIRE(result.m_compacted_turn_count == 2);
    REQUIRE(result.m_recent_turns.size() == 1);
    REQUIRE(result.m_recent_turns.front().m_user_text == "recent question");
    REQUIRE(result.m_recent_turns.front().m_assistant_text == "recent answer");
    REQUIRE(result.m_summary_text.find("old question one") != std::string::npos);
    REQUIRE(result.m_summary_text.find("old answer two") != std::string::npos);
    REQUIRE(result.m_summary_text.size() <= 200);
}

TEST_CASE("conversation compaction is bounded and never changes source history", "[context]")
{
    const std::vector<fcpp::SConversationTurn> history{
        {2, std::string(300, 'u'), std::string(300, 'a')},
        {4, "latest question", "latest answer"}
    };

    const auto result = fcpp::compact_conversation_history(history, 1, 40);

    REQUIRE(result.m_compacted_turn_count == 1);
    REQUIRE(result.m_summary_text.size() == 40);
    REQUIRE(result.m_recent_turns.front().m_user_text == "latest question");
    REQUIRE(history.front().m_user_text.size() == 300);
    REQUIRE(history.front().m_assistant_text.size() == 300);
}

TEST_CASE("conversation compaction does not summarize when all turns fit the recent window", "[context]")
{
    const std::vector<fcpp::SConversationTurn> history{
        {2, "question", "answer"}
    };

    const auto result = fcpp::compact_conversation_history(history, 8, 100);

    REQUIRE(result.m_compacted_turn_count == 0);
    REQUIRE(result.m_summary_text.empty());
    REQUIRE(result.m_recent_turns.size() == 1);
}

TEST_CASE("context manager selects high-priority groups within budget", "[context]")
{
    const fcpp::CContextManager manager(
        [](const fcpp::SModelMessage& message) { return message.m_content.size(); });
    const std::vector<fcpp::SContextGroup> groups{
        group("system", "sys", 0, 0, true),
        group("old", "123456", 1, 1),
        group("relevant", "abcd", 2, 10),
        group("recent", "xyz", 3, 5)
    };

    const auto result = manager.build(groups, {10});
    REQUIRE(result.m_ok);
    REQUIRE(result.m_estimated_tokens == 10);
    REQUIRE((result.m_included_group_ids == std::vector<std::string>{"system", "relevant", "recent"}));
    REQUIRE(result.m_excluded_group_ids == std::vector<std::string>{"old"});
    REQUIRE(result.m_messages.size() == 3);
    REQUIRE(result.m_messages.front().m_content == "sys");
}

TEST_CASE("context manager never partially includes an atomic message group", "[context]")
{
    const fcpp::CContextManager manager(
        [](const fcpp::SModelMessage& message) { return message.m_content.size(); });
    auto pair = group("tool-pair", "call", 1, 100);
    pair.m_messages.push_back({fcpp::EModelRole::Tool, "result", {}, {}});
    const auto result = manager.build({group("system", "s", 0, 0, true), pair}, {8});

    REQUIRE(result.m_ok);
    REQUIRE(result.m_included_group_ids == std::vector<std::string>{"system"});
    REQUIRE(result.m_excluded_group_ids == std::vector<std::string>{"tool-pair"});
    REQUIRE(result.m_messages.size() == 1);
}

TEST_CASE("context manager fails clearly when mandatory context does not fit", "[context]")
{
    const fcpp::CContextManager manager(
        [](const fcpp::SModelMessage& message) { return message.m_content.size(); });
    const auto result = manager.build({group("policy", "too-long", 0, 0, true)}, {2});

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_messages.empty());
    REQUIRE(result.m_error == "mandatory context exceeds prompt token budget");
}

TEST_CASE("context manager rejects an unspecified budget", "[context]")
{
    const fcpp::CContextManager manager;
    const auto result = manager.build({group("user", "hello", 0, 1)}, {0});
    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_error == "prompt token budget is zero");
}


TEST_CASE("context manager rejects duplicate group ids", "[context]")
{
    const fcpp::CContextManager manager;
    const auto result = manager.build(
        {group("same", "first", 0, 1), group("same", "second", 1, 1)}, {100});

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_error == "context group ids must be unique");
}

TEST_CASE("context manager estimates each message exactly once", "[context]")
{
    std::size_t calls = 0;
    const fcpp::CContextManager manager(
        [&calls](const fcpp::SModelMessage& message)
        {
            ++calls;
            return message.m_content.size();
        });
    const auto result = manager.build(
        {group("mandatory", "sys", 0, 100, true), group("optional", "hello", 1, 10)}, {100});

    REQUIRE(result.m_ok);
    REQUIRE(calls == 2);
}


TEST_CASE("context manager reserves completion tokens from the model context window", "[context]")
{
    const fcpp::CContextManager manager(
        [](const fcpp::SModelMessage& message) { return message.m_content.size(); });
    const auto result = manager.build(
        {group("system", "policy", 0, 0, true), group("history", std::string(40, 'x'), 1, 1)},
        {0, 64, 16});

    REQUIRE(result.m_ok);
    REQUIRE(result.m_budget_tokens == 48);
    REQUIRE(result.m_estimated_tokens == 46);
}

TEST_CASE("context manager rejects an output reserve that consumes the full context window", "[context]")
{
    const fcpp::CContextManager manager;
    const auto result = manager.build({group("user", "hello", 0, 1)}, {0, 4096, 4096});

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_error == "output reserve must be smaller than model context window");
}

TEST_CASE("context manager applies the stricter of prompt cap and model window", "[context]")
{
    const fcpp::CContextManager manager(
        [](const fcpp::SModelMessage& message) { return message.m_content.size(); });
    const auto result = manager.build(
        {group("mandatory", std::string(30, 'x'), 0, 1, true)}, {40, 24, 8});

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_budget_tokens == 16);
    REQUIRE(result.m_error == "mandatory context exceeds prompt token budget");
}
