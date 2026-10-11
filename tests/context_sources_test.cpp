#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "core/CChatContextSource.h"
#include "core/CCodeContextSource.h"
#include "core/CContextManager.h"
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

TEST_CASE("combined context keeps indexed code ahead of chat when the prompt budget is tight", "[context][code][chat]")
{
    fcpp::SEvidence evidence;
    evidence.m_sections.push_back({
        fcpp::EToolName::Source, "Code", std::string(18, 'c')});
    const fcpp::CCodeContextSource code_source(evidence);
    const auto code = code_source.collect_context({"inspect code", 1000});
    REQUIRE(code.m_groups.size() == 1);

    const std::vector<fcpp::SConversationTurn> history{{1, "x", ""}};
    const fcpp::CChatContextSource chat_source(history);
    const auto chat = chat_source.collect_context({"inspect code", 1000});
    REQUIRE(chat.m_groups.size() == 1);

    fcpp::SContextGroup current_question;
    current_question.m_id = "current-question";
    current_question.m_order = 0;
    current_question.m_mandatory = true;
    current_question.m_messages.push_back(
        {fcpp::EModelRole::User, "q", {}, {}});

    std::vector<fcpp::SContextGroup> groups{current_question};
    fcpp::SContextGroup code_group = code.m_groups.front();
    code_group.m_order = 1;
    groups.push_back(code_group);
    fcpp::SContextGroup chat_group = chat.m_groups.front();
    chat_group.m_order = 2;
    groups.push_back(chat_group);

    const fcpp::CContextManager manager(
        [](const fcpp::SModelMessage& message) { return message.m_content.size(); });
    const auto built = manager.build(groups, {30});

    REQUIRE(built.m_ok);
    REQUIRE(std::find(built.m_included_group_ids.begin(),
                      built.m_included_group_ids.end(), "code-evidence-1")
            != built.m_included_group_ids.end());
    REQUIRE(std::find(built.m_included_group_ids.begin(),
                      built.m_included_group_ids.end(), "chat-history-1")
            == built.m_included_group_ids.end());
    REQUIRE(std::find(built.m_excluded_group_ids.begin(),
                      built.m_excluded_group_ids.end(), "chat-history-1")
            != built.m_excluded_group_ids.end());
}
