#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "core/CModelCascade.h"
#include "fcpp-agent/SCascade.h"

TEST_CASE("cascade: успех на первом уровне", "[cascade]")
{
    const fcpp::SCascadeResult result = fcpp::CModelCascade::run(
        {fcpp::EModelLevel::Lite, fcpp::EModelLevel::Main},
        [](fcpp::EModelLevel, const std::string&) -> fcpp::SCascadeAttempt
        {
            fcpp::SCascadeAttempt attempt;
            attempt.m_valid = true;
            attempt.m_result_text = "ok";
            return attempt;
        },
        5);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_level == fcpp::EModelLevel::Lite);
    REQUIRE(result.m_attempts == 1);
    REQUIRE(result.m_glitch == fcpp::EGlitchKind::None);
}

TEST_CASE("cascade: эскалация lite->main после глюка", "[cascade]")
{
    std::vector<fcpp::EModelLevel> visited;

    const fcpp::SCascadeResult result = fcpp::CModelCascade::run(
        {fcpp::EModelLevel::Lite, fcpp::EModelLevel::Main},
        [&visited](fcpp::EModelLevel level, const std::string& ctx) -> fcpp::SCascadeAttempt
        {
            visited.push_back(level);
            fcpp::SCascadeAttempt attempt;
            if (level == fcpp::EModelLevel::Main)
            {
                attempt.m_valid = true;
                attempt.m_result_text = "main ok";
                return attempt;
            }
            attempt.m_valid = false;
            attempt.m_glitch = fcpp::EGlitchKind::Syntax;
            attempt.m_error_context = "bad xml" + ctx;
            return attempt;
        },
        5);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_level == fcpp::EModelLevel::Main);
    // lite: 1 попытка + 1 ретрай, затем main: успех -> 3 попытки.
    REQUIRE(result.m_attempts == 3);
}

TEST_CASE("cascade: hard-stop при исчерпании всех уровней", "[cascade]")
{
    const fcpp::SCascadeResult result = fcpp::CModelCascade::run(
        {fcpp::EModelLevel::Lite, fcpp::EModelLevel::Main, fcpp::EModelLevel::Remote},
        [](fcpp::EModelLevel, const std::string&) -> fcpp::SCascadeAttempt
        {
            fcpp::SCascadeAttempt attempt;
            attempt.m_valid = false;
            attempt.m_glitch = fcpp::EGlitchKind::Syntax;
            attempt.m_error_context = "still bad";
            return attempt;
        },
        5);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_level == fcpp::EModelLevel::Remote);
    // 2 попытки на уровень * 3 уровня = 6, но бюджет 5 -> hard-stop на бюджете.
    REQUIRE(result.m_attempts == 5);
    REQUIRE(result.m_glitch == fcpp::EGlitchKind::Loop);
}

TEST_CASE("cascade: невалидный ретрай ограничен бюджетом (без циклов)", "[cascade]")
{
    std::uint32_t calls = 0;
    const fcpp::SCascadeResult result = fcpp::CModelCascade::run(
        {fcpp::EModelLevel::Lite},
        [&calls](fcpp::EModelLevel, const std::string&) -> fcpp::SCascadeAttempt
        {
            ++calls;
            fcpp::SCascadeAttempt attempt;
            attempt.m_valid = false;
            attempt.m_glitch = fcpp::EGlitchKind::Schema;
            return attempt;
        },
        10);

    REQUIRE_FALSE(result.m_ok);
    // Ровно 1 попытка + 1 ретрай на единственном уровне; не бесконечный цикл.
    REQUIRE(calls == 2);
    REQUIRE(result.m_attempts == 2);
}

TEST_CASE("cascade: stops retry after visible stream output", "[cascade][stream]")
{
    std::uint32_t calls = 0;
    const fcpp::SCascadeResult result = fcpp::CModelCascade::run(
        {fcpp::EModelLevel::Lite, fcpp::EModelLevel::Main},
        [&calls](fcpp::EModelLevel, const std::string&) -> fcpp::SCascadeAttempt
        {
            ++calls;
            fcpp::SCascadeAttempt attempt;
            attempt.m_glitch = fcpp::EGlitchKind::Provider;
            attempt.m_error_context = "provider failed after first chunk";
            attempt.m_stream_started = true;
            return attempt;
        },
        5);
    REQUIRE_FALSE(result.m_ok);
    REQUIRE(calls == 1);
    REQUIRE(result.m_attempts == 1);
}
