#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/CCodeContextSource.h"
#include "core/CProjectProfileContextSource.h"

TEST_CASE("project passport source respects its allowance", "[context][passport]")
{
    const std::string profile = "Use C++20. Keep public APIs stable.";
    const fcpp::CProjectProfileContextSource source(profile);

    const auto result = source.collect_context({"implement the requested change", 200});

    REQUIRE(result.m_complete);
    REQUIRE(result.m_groups.size() == 1);
    REQUIRE(result.m_groups.front().m_id == "project-passport");
    REQUIRE(result.m_estimated_tokens <= 200);
}

TEST_CASE("project passport source reports when its content cannot fit", "[context][passport]")
{
    const std::string profile(4000, 'p');
    const fcpp::CProjectProfileContextSource source(profile);

    const auto result = source.collect_context({"task", 10});

    REQUIRE_FALSE(result.m_complete);
    REQUIRE(result.m_groups.empty());
    REQUIRE_FALSE(result.m_note.empty());
}

TEST_CASE("code evidence source prioritizes indexed evidence and enforces its allowance", "[context][code]")
{
    fcpp::SEvidence evidence;
    evidence.m_sections.push_back({fcpp::EToolName::Outline, "Class outline", "class A {};"});
    evidence.m_sections.push_back({fcpp::EToolName::Source, "Implementation", "void A::run() {}"});

    const fcpp::CCodeContextSource source(evidence);
    const auto result = source.collect_context({"inspect A::run", 300});

    REQUIRE(result.m_complete);
    REQUIRE(result.m_groups.size() == 2);
    REQUIRE(result.m_estimated_tokens <= 300);
    REQUIRE(result.m_groups.front().m_priority == 110);
}
