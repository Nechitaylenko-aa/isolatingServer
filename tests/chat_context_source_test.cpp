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


TEST_CASE("chat source preserves previous user and assistant turns for follow-up resolution", "[context][chat]")
{
    const std::vector<fcpp::SConversationTurn> history{
        {2, "Тут есть ошибки в методе CLogger::log_event?", "Обсуждаем метод CLogger::log_event."},
        {4, "А что он делает?", "Метод CLogger::log_event записывает диагностическое событие."}
    };

    const fcpp::CChatContextSource source(history);
    const auto result = source.collect_context({"resolve the antecedent of a follow-up", 200});

    REQUIRE(result.m_complete);
    REQUIRE_FALSE(result.m_groups.empty());

    std::string combined;
    for (const fcpp::SContextGroup& group : result.m_groups)
    {
        for (const fcpp::SModelMessage& message : group.m_messages)
        {
            combined += message.m_content;
            combined += "\n";
        }
    }
    REQUIRE(combined.find("CLogger::log_event") != std::string::npos);
    REQUIRE(combined.find("А что он делает?") != std::string::npos);
    REQUIRE(combined.find("записывает диагностическое событие") != std::string::npos);
}
