/**
 * @file context_budget_test.cpp
 * @brief Распил контекстного бюджета: запрос целиком, остаток 70/30.
 */

#include "core/CContextBudget.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("budget: request fits, remainder split 70/30", "[budget]")
{
    // window 10000, reserve 2000 → prompt budget 8000, request 1000.
    const fcpp::SContextBudgetSplit split =
        fcpp::split_context_budget(10000, 2000, 1000);

    REQUIRE(split.m_ok);
    REQUIRE(split.m_window == 10000);
    REQUIRE(split.m_reserve == 2000);
    REQUIRE(split.m_total == 8000);
    REQUIRE(split.m_request == 1000);
    REQUIRE(split.m_remaining == 7000);
    REQUIRE(split.m_cursor == 4900);   // 70%
    REQUIRE(split.m_history == 2100);  // 30%
    REQUIRE(split.m_cursor + split.m_history == split.m_remaining);
}

TEST_CASE("budget: request that does not fit is rejected, not truncated",
          "[budget]")
{
    // The whole budget is 8000, the request alone asks for 8000.
    const fcpp::SContextBudgetSplit split =
        fcpp::split_context_budget(10000, 2000, 8000);

    REQUIRE_FALSE(split.m_ok);
    REQUIRE_FALSE(split.m_error.empty());
    REQUIRE(split.m_error.find("does not fit") != std::string::npos);
    REQUIRE(split.m_cursor == 0);
    REQUIRE(split.m_history == 0);
}

TEST_CASE("budget: request larger than the budget is rejected", "[budget]")
{
    const fcpp::SContextBudgetSplit split =
        fcpp::split_context_budget(8192, 4096, 50000);

    REQUIRE_FALSE(split.m_ok);
    REQUIRE(split.m_error.find("does not fit") != std::string::npos);
}

TEST_CASE("budget: reserve equal to window leaves no prompt room", "[budget]")
{
    const fcpp::SContextBudgetSplit split =
        fcpp::split_context_budget(4096, 4096, 10);

    REQUIRE_FALSE(split.m_ok);
    REQUIRE_FALSE(split.m_error.empty());
}

TEST_CASE("budget: unknown window is refused, not treated as unlimited",
          "[budget]")
{
    const fcpp::SContextBudgetSplit split =
        fcpp::split_context_budget(0, 0, 10);

    REQUIRE_FALSE(split.m_ok);
    REQUIRE(split.m_error.find("window is unknown") != std::string::npos);
}

TEST_CASE("budget: remote contract window gives a large prompt allowance",
          "[budget]")
{
    // 56K window, 8K reserve: the prompt gets 48K, code 70% of what remains.
    const fcpp::SContextBudgetSplit split =
        fcpp::split_context_budget(57344, 8192, 500);

    REQUIRE(split.m_ok);
    REQUIRE(split.m_total == 49152);
    REQUIRE(split.m_remaining == 48652);
    REQUIRE(split.m_cursor == 34056);  // 70% of 48652
    REQUIRE(split.m_history == 14595); // 30% of 48652
}
