#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include <string>
#include <vector>

#include "core/CChatContextSource.h"

TEST_CASE("chat source keeps its contribution within the requested token allowance", "[context][chat]")
{
    const std::vector<fcpp::SConversationTurn> history{
        {2, std::string(20, 'a'), std::string(20, 'b')},
        {4, std::string(20, 'c'), std::string(20, 'd')},
        {6, std::string(20, 'e'), std::string(20, 'f')}
    };

    const fcpp::CChatContextSource source(history);
    const auto result = source.collect_context({"inspect the latest turn", 40});

    REQUIRE(result.m_estimated_tokens <= 40);
    REQUIRE_FALSE(result.m_groups.empty());
    REQUIRE(std::any_of(result.m_groups.begin(), result.m_groups.end(),
        [](const fcpp::SContextGroup& group)
        {
            return group.m_id == "chat-history-3";
        }));
    REQUIRE(history.size() == 3);
}

TEST_CASE("chat source reports a zero allowance without returning history", "[context][chat]")
{
    const std::vector<fcpp::SConversationTurn> history{{2, "question", "answer"}};
    const fcpp::CChatContextSource source(history);

    const auto result = source.collect_context({"task", 0});

    REQUIRE(result.m_groups.empty());
    REQUIRE_FALSE(result.m_complete);
    REQUIRE(result.m_note == "chat source allowance is zero");
}
