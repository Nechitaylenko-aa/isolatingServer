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
