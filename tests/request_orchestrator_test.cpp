#include "core/CRequestOrchestrator.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/CLogger.h"

namespace fcpp
{
namespace
{

TEST_CASE("request orchestrator walks stub tree and returns terminal reason")
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
    REQUIRE(result.m_status == "STUB_COMPLETE");
    REQUIRE(result.m_exit_reason == "SCOPE_CLASSIFICATION_NOT_IMPLEMENTED");
    REQUIRE(result.m_route == "NO_APPLICABLE_CONTEXT");
    REQUIRE(result.m_trace.size() == 8);
    REQUIRE(result.m_trace.back() == "EXIT:SCOPE_CLASSIFICATION_NOT_IMPLEMENTED");
    REQUIRE(result.m_message.find("ORCHESTRATION_STATUS=STUB_COMPLETE") != std::string::npos);
    REQUIRE(result.m_message.find("EXIT_REASON=SCOPE_CLASSIFICATION_NOT_IMPLEMENTED") != std::string::npos);

    std::ifstream log_file(logger.path());
    REQUIRE(log_file.is_open());
    const std::string log_text((std::istreambuf_iterator<char>(log_file)),
                               std::istreambuf_iterator<char>());
    REQUIRE(log_text.find("orchestrator.started") != std::string::npos);
    REQUIRE(log_text.find("orchestrator.stage") != std::string::npos);
    REQUIRE(log_text.find("orchestrator.finished") != std::string::npos);
    REQUIRE(log_text.find("orchestrator-test-001") != std::string::npos);

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
    REQUIRE(result.m_status == "REJECTED");
    REQUIRE(result.m_exit_reason == "EMPTY_REQUEST");
    REQUIRE(result.m_trace.back() == "EXIT:EMPTY_REQUEST");

    std::ifstream log_file(logger.path());
    REQUIRE(log_file.is_open());
    const std::string log_text((std::istreambuf_iterator<char>(log_file)),
                               std::istreambuf_iterator<char>());
    REQUIRE(log_text.find("EMPTY_REQUEST") != std::string::npos);

    std::filesystem::remove_all(log_dir, ignored);
}

} // namespace
} // namespace fcpp
