#include "core/CRequestOrchestrator.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/CLogger.h"
#include "fcpp-agent/SLogEvent.h"

namespace fcpp
{
namespace
{
TEST_CASE("request orchestrator reports missing LLM layer for no-context route")
{
    const std::filesystem::path log_dir =
        std::filesystem::temp_directory_path() / "fcpp-agent-orchestrator-test";
    std::error_code ignored;
    std::filesystem::remove_all(log_dir, ignored);

    CLogger logger(log_dir, ELogLevel::Trace);
    CRequestOrchestrator orchestrator(logger);

    SOrchestrationRequest request;
    request.m_request_id = "orchestrator-test-001";
    request.m_session_id = "session-test";
    request.m_text = "Explain this request";

    const SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_status == ERequestStatus::Failed);
    REQUIRE(result.m_exit_reason == EExitReason::LlmGenerationFailed);
    REQUIRE(result.m_route == EOrchestrationRoute::NoApplicableContext);
    REQUIRE(result.m_trace.size() == 4);
    REQUIRE(result.m_trace.back() == ETraceEvent::ExitLlmGenerationFailed);
    REQUIRE(result.m_message.find("LLM layer is not configured") != std::string::npos);

    std::ifstream log_file(logger.path());
    REQUIRE(log_file.is_open());
    const std::string log_text((std::istreambuf_iterator<char>(log_file)),
                               std::istreambuf_iterator<char>());
    REQUIRE(log_text.find("request.started") != std::string::npos);
    REQUIRE(log_text.find("request.finished") != std::string::npos);
    REQUIRE(log_text.find("orchestrator.stage") == std::string::npos);
    REQUIRE(log_text.find("orchestrator-test-001") != std::string::npos);
    REQUIRE(log_text.find(R"("text_length":20)") != std::string::npos);
    REQUIRE(log_text.find(R"("elapsed_ms":)") != std::string::npos);

    std::filesystem::remove_all(log_dir, ignored);
}

TEST_CASE("request orchestrator rejects an empty request and logs the exit")
{
    const std::filesystem::path log_dir =
        std::filesystem::temp_directory_path() / "fcpp-agent-orchestrator-empty-test";
    std::error_code ignored;
    std::filesystem::remove_all(log_dir, ignored);

    CLogger logger(log_dir, ELogLevel::Trace);
    CRequestOrchestrator orchestrator(logger);

    SOrchestrationRequest request;
    request.m_request_id = "orchestrator-test-empty";

    const SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_status == ERequestStatus::Rejected);
    REQUIRE(result.m_exit_reason == EExitReason::EmptyRequest);
    REQUIRE(result.m_trace.back() == ETraceEvent::ExitEmptyRequest);

    std::ifstream log_file(logger.path());
    REQUIRE(log_file.is_open());
    const std::string log_text((std::istreambuf_iterator<char>(log_file)),
                               std::istreambuf_iterator<char>());
    REQUIRE(log_text.find("EMPTY_REQUEST") != std::string::npos);

    std::filesystem::remove_all(log_dir, ignored);
}

TEST_CASE("logger expands typed log events into structured JSONL")
{
    const std::filesystem::path log_dir =
        std::filesystem::temp_directory_path() / "fcpp-agent-typed-log-event-test";
    std::error_code ignored;
    std::filesystem::remove_all(log_dir, ignored);

    CLogger logger(log_dir, ELogLevel::Trace);
    logger.log(ELogLevel::Warn,
        SLogEvent::llm_escalated("THINKER_LOCAL", "THINKER_SMART", "INVALID_OUTPUT"));

    std::ifstream log_file(logger.path());
    REQUIRE(log_file.is_open());
    const std::string log_text((std::istreambuf_iterator<char>(log_file)),
                               std::istreambuf_iterator<char>());
    REQUIRE(log_text.find(R"("event":"llm.escalated")") != std::string::npos);
    REQUIRE(log_text.find(R"("from_role":"THINKER_LOCAL")") != std::string::npos);
    REQUIRE(log_text.find(R"("to_role":"THINKER_SMART")") != std::string::npos);

    std::filesystem::remove_all(log_dir, ignored);
}
} // namespace
} // namespace fcpp
