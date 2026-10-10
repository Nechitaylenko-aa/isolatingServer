/**
 * @file subtask_test.cpp
 * @brief Тесты декомпозиции составного запроса — Фаза A (ADR-16/ADR-17).
 */

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/CSubtaskComposer.h"
#include "fcpp-agent/SSubtasks.h"

TEST_CASE("composer: один подвопрос не является составным", "[subtask]")
{
    const std::string text = "Что делает этот класс?";

    fcpp::SSubtaskPlan plan;
    std::string error;
    REQUIRE(fcpp::CSubtaskComposer::parse_subtask_list(text, 6, plan, error));
    REQUIRE(plan.size() == 1);
    REQUIRE_FALSE(plan.m_compound);
    REQUIRE(plan.m_subtasks.front().m_question == "Что делает этот класс?");
}

TEST_CASE("composer: несколько строк — составной; маркеры списка снимаются", "[subtask]")
{
    const std::string text =
        "1. Какие базовые классы с virtual методами?\n"
        "2. Какие классы наследуются от них?\n"
        "- Переопределены ли все чисто виртуальные методы?\n";

    fcpp::SSubtaskPlan plan;
    std::string error;
    REQUIRE(fcpp::CSubtaskComposer::parse_subtask_list(text, 6, plan, error));
    REQUIRE(plan.size() == 3);
    REQUIRE(plan.m_compound);
    REQUIRE(plan.m_subtasks[0].m_question == "Какие базовые классы с virtual методами?");
    REQUIRE(plan.m_subtasks[2].m_question == "Переопределены ли все чисто виртуальные методы?");
}

TEST_CASE("composer: лимит числа подвопросов соблюдается", "[subtask]")
{
    const std::string text = "первый\nвторой\nтретий\n";

    fcpp::SSubtaskPlan plan;
    std::string error;
    REQUIRE(fcpp::CSubtaskComposer::parse_subtask_list(text, 2, plan, error));
    REQUIRE(plan.size() == 2);
}

TEST_CASE("composer: пустой текст -> ошибка", "[subtask]")
{
    fcpp::SSubtaskPlan plan;
    std::string error;
    REQUIRE_FALSE(fcpp::CSubtaskComposer::parse_subtask_list("\n\n", 6, plan, error));
    REQUIRE_FALSE(error.empty());
}
