#include "core/CHistorian.h"
#include "core/CCursorContextAnalyzer.h"
#include "core/CLogger.h"
#include "core/CRequestOrchestrator.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/IClang.h"

namespace
{
class CFixedReplyModel final : public fcpp::IModelApi
{
public:
    CFixedReplyModel(
        std::string text,
        std::vector<std::vector<fcpp::SModelMessage>>* captured_messages = nullptr)
        : m_text(std::move(text))
        , m_captured_messages(captured_messages)
    {
    }

    bool is_available() const override { return true; }

    fcpp::SModelReply complete(
        const std::vector<fcpp::SModelMessage>& messages) override
    {
        if (m_captured_messages != nullptr)
            m_captured_messages->push_back(messages);
        return fcpp::SModelReply{true, m_text, {}, "stop", {}};
    }

    fcpp::SModelReply complete(
        const std::vector<fcpp::SModelMessage>& messages,
        const std::vector<fcpp::SModelTool>&) override
    {
        return complete(messages);
    }

    fcpp::SModelReply complete_stream(
        const std::vector<fcpp::SModelMessage>& messages,
        const fcpp::SStreamCallback&) override
    {
        return complete(messages);
    }

private:
    std::string m_text;
    std::vector<std::vector<fcpp::SModelMessage>>* m_captured_messages;
};

class CContextAwareModel final : public fcpp::IModelApi
{
public:
    explicit CContextAwareModel(fcpp::ELlmRole role) : m_role(role) {}
    bool is_available() const override { return true; }

    fcpp::SModelReply complete(
        const std::vector<fcpp::SModelMessage>& messages) override
    {
        if (m_role == fcpp::ELlmRole::CoderSmart)
            return {true, "Combined context answer", {}, "stop", {}};
        const std::string system = messages.empty() ? std::string{} : messages.front().m_content;
        if (system.find("Select the prior conversation turns") != std::string::npos)
            return {true, "1", {}, "stop", {}};
        if (system.find("Decide whether the C++ entity") != std::string::npos)
            return {true, "YES", {}, "stop", {}};
        return {true, "invalid", {}, "stop", {}};
    }

    fcpp::SModelReply complete(
        const std::vector<fcpp::SModelMessage>& messages,
        const std::vector<fcpp::SModelTool>&) override { return complete(messages); }
    fcpp::SModelReply complete_stream(
        const std::vector<fcpp::SModelMessage>& messages,
        const fcpp::SStreamCallback&) override { return complete(messages); }

private:
    fcpp::ELlmRole m_role;
};

class CCursorTestClang final : public fcpp::IClang
{
public:
    explicit CCursorTestClang(fcpp::SClangSymbol symbol) : m_symbol(std::move(symbol)) {}
    bool is_available() const override { return true; }
    std::string version() const override { return "test"; }
    bool extract(const std::filesystem::path&, const std::vector<std::string>&,
                 fcpp::SClangExtract&) const override { return true; }
    std::optional<fcpp::SClangSymbol> cursor_at(
        const std::filesystem::path&, std::uint32_t, std::uint32_t,
        const std::vector<std::string>&,
        fcpp::EFocusKind = fcpp::EFocusKind::Innermost) const override { return m_symbol; }
private:
    fcpp::SClangSymbol m_symbol;
};

std::vector<fcpp::SConversationTurn> sample_history()
{
    return {
        {1, "How is the project configured?", "It uses CMake."},
        {2, "Remember the red bicycle", "I will keep that in context."},
        {3, "Which tests cover parsing?", "The parser tests cover malformed input."}
    };
}

std::filesystem::path log_path(const std::string& suffix)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("fcpp-historian-test-" + suffix);
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
    return path;
}
} // namespace

TEST_CASE("historian selection accepts a comma-separated list of valid turn numbers",
          "[historian][selection]")
{
    std::vector<std::size_t> indices;
    std::string error;
    REQUIRE(fcpp::CHistorian::decode_relevant_indices("1, 3", 3, indices, error));
    REQUIRE(indices == std::vector<std::size_t>{0, 2});
}

TEST_CASE("historian selection accepts NONE as an empty context",
          "[historian][selection]")
{
    std::vector<std::size_t> indices{0};
    std::string error;
    REQUIRE(fcpp::CHistorian::decode_relevant_indices("NONE", 3, indices, error));
    REQUIRE(indices.empty());
}

TEST_CASE("historian selection rejects malformed or unsafe indices",
          "[historian][selection]")
{
    const std::vector<std::string> invalid{
        "", "none", "0", "4", "1, 1", "1,", "1 2", "1, x", "1,2 explanation"
    };
    for (const std::string& text : invalid)
    {
        DYNAMIC_SECTION("rejects: " << text)
        {
            std::vector<std::size_t> indices;
            std::string error;
            REQUIRE_FALSE(fcpp::CHistorian::decode_relevant_indices(
                text, 3, indices, error));
            REQUIRE_FALSE(error.empty());
        }
    }
}

TEST_CASE("historian asks the local thinker to select numbered question-answer pairs",
          "[historian][selection][llm]")
{
    fcpp::SLlmRoleSet roles;
    std::vector<fcpp::ELlmRole> called_roles;
    std::vector<std::vector<fcpp::SModelMessage>> captured_messages;
    fcpp::CLogger logger(log_path("valid"), fcpp::ELogLevel::Trace);
    fcpp::CLlmLayer llm(roles, logger,
        [&](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            called_roles.push_back(role);
            return std::make_unique<CFixedReplyModel>("1, 3", &captured_messages);
        });
    fcpp::CHistorian historian(llm);

    const fcpp::SHistoryRelevance result =
        historian.assess_relevance("Which tests cover parsing?", sample_history());

    REQUIRE(result.m_ok);
    REQUIRE(result.m_history_needed);
    REQUIRE_FALSE(result.m_escalated);
    REQUIRE(result.m_used_role == fcpp::ELlmRole::ThinkerLocal);
    REQUIRE(result.m_relevant_turn_indices == std::vector<std::size_t>{0, 2});
    REQUIRE(called_roles == std::vector<fcpp::ELlmRole>{fcpp::ELlmRole::ThinkerLocal});
    REQUIRE(captured_messages.size() == 1);
    REQUIRE(captured_messages[0].size() == 2);
    REQUIRE(captured_messages[0][0].m_content.find("1-based numbers") != std::string::npos);
    REQUIRE(captured_messages[0][1].m_content.find("[1]") != std::string::npos);
    REQUIRE(captured_messages[0][1].m_content.find("Remember the red bicycle") != std::string::npos);
    REQUIRE(captured_messages[0][1].m_content.find("Which tests cover parsing?") != std::string::npos);
}

TEST_CASE("historian escalates malformed local output and accepts the smart model result",
          "[historian][selection][llm][escalation]")
{
    fcpp::SLlmRoleSet roles;
    std::vector<fcpp::ELlmRole> called_roles;
    fcpp::CLogger logger(log_path("escalation"), fcpp::ELogLevel::Trace);
    fcpp::CLlmLayer llm(roles, logger,
        [&](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            called_roles.push_back(role);
            const std::string answer = role == fcpp::ELlmRole::ThinkerLocal
                ? "The relevant pair is number two."
                : "2";
            return std::make_unique<CFixedReplyModel>(answer);
        });
    fcpp::CHistorian historian(llm);

    const fcpp::SHistoryRelevance result =
        historian.assess_relevance("What did I ask you to remember?", sample_history());

    REQUIRE(result.m_ok);
    REQUIRE(result.m_escalated);
    REQUIRE(result.m_used_role == fcpp::ELlmRole::ThinkerSmart);
    REQUIRE(result.m_relevant_turn_indices == std::vector<std::size_t>{1});
    REQUIRE(called_roles == std::vector<fcpp::ELlmRole>{
        fcpp::ELlmRole::ThinkerLocal, fcpp::ELlmRole::ThinkerSmart});
}

TEST_CASE("historian reports an error when both model roles violate the selection contract",
          "[historian][selection][llm][error]")
{
    fcpp::SLlmRoleSet roles;
    fcpp::CLogger logger(log_path("failure"), fcpp::ELogLevel::Trace);
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CFixedReplyModel>("probably pair two");
        });
    fcpp::CHistorian historian(llm);

    const fcpp::SHistoryRelevance result =
        historian.assess_relevance("What did we discuss?", sample_history());

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_escalated);
    REQUIRE_FALSE(result.m_error.empty());
    REQUIRE(result.m_relevant_turn_indices.empty());
}

TEST_CASE("orchestrator returns only the history pairs selected by the historian",
          "[orchestrator][historian]")
{
    fcpp::SLlmRoleSet roles;
    fcpp::CLogger logger(log_path("orchestrator"), fcpp::ELogLevel::Trace);
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CFixedReplyModel>("2");
        });
    fcpp::CHistorian historian(llm);
    fcpp::CRequestOrchestrator orchestrator(logger, &historian, nullptr, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "historian-context-test";
    request.m_session_id = "historian-session";
    request.m_text = "What did I ask you to remember?";
    request.m_history = sample_history();

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::HistoryContextRequired);
    REQUIRE(result.m_history_context.size() == 1);
    REQUIRE(result.m_history_context[0].m_user_text == "Remember the red bicycle");
    REQUIRE(result.m_history_context[0].m_assistant_text == "I will keep that in context.");
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::ScopeClassificationNotImplemented);
}


TEST_CASE("combined cursor and history route sends both selected contexts to smart C++ coder",
          "[orchestrator][cursor][historian]")
{
    const auto directory = log_path("combined-context");
    const auto source_file = directory / "sample.cpp";
    std::filesystem::create_directories(directory);
    {
        std::ofstream source(source_file);
        REQUIRE(source.is_open());
        source << "class CWidget {\\npublic:\\n    int value() const { return 42; }\\n};\\n";
    }

    fcpp::SClangSymbol symbol{};
    symbol.m_name = "CWidget";
    symbol.m_qualified_name = "CWidget";
    symbol.m_signature = "class CWidget";
    symbol.m_kind = fcpp::ESymbolKind::Class;
    symbol.m_location.m_file = source_file.string();
    symbol.m_location.m_line = 1;
    symbol.m_location.m_column = 1;
    symbol.m_location.m_end_line = 4;
    symbol.m_is_definition = true;

    fcpp::SLlmRoleSet roles;
    std::vector<std::vector<fcpp::SModelMessage>> coder_messages;
    fcpp::CLogger logger(log_path("combined-context-logs"), fcpp::ELogLevel::Trace);
    fcpp::CLlmLayer llm(roles, logger,
        [&](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            if (role == fcpp::ELlmRole::CoderSmart)
                return std::make_unique<CFixedReplyModel>("Combined context answer", &coder_messages);
            return std::make_unique<CContextAwareModel>(role);
        });
    CCursorTestClang clang(symbol);
    fcpp::CCursorContextAnalyzer analyzer(clang, llm);
    fcpp::CHistorian historian(llm);
    fcpp::CRequestOrchestrator orchestrator(logger, &historian, &analyzer, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "combined-context-test";
    request.m_text = "Explain this class and remember my earlier constraint";
    request.m_cursor_file = source_file.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;
    request.m_history = {
        {1, "The earlier constraint", "Keep the method const."},
        {2, "Unrelated topic", "A different answer."}
    };

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_status == fcpp::ERequestStatus::Completed);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::CursorAndHistoryContextRequired);
    REQUIRE(result.m_history_context.size() == 1);
    REQUIRE(result.m_history_context.front().m_user_text == "The earlier constraint");
    REQUIRE(result.m_message == "Combined context answer");
    REQUIRE(coder_messages.size() == 1);
    REQUIRE(coder_messages.front().size() == 2);
    REQUIRE(coder_messages.front()[0].m_content.find("senior C++ engineer") != std::string::npos);
    REQUIRE(coder_messages.front()[1].m_content.find("class CWidget") != std::string::npos);
    REQUIRE(coder_messages.front()[1].m_content.find("Keep the method const.") != std::string::npos);
    REQUIRE(coder_messages.front()[1].m_content.find(request.m_text) != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}
