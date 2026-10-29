/**
 * @file CRequestOrchestrator.cpp
 * @brief Explicit, instrumented request-processing tree skeleton.
 */

#include "core/CRequestOrchestrator.h"
#include "core/CHistorian.h"
#include "core/CCursorContextAnalyzer.h"

#include <chrono>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/CLogger.h"
#include "fcpp-agent/SLogEvent.h"

namespace fcpp
{

CRequestOrchestrator::CRequestOrchestrator(CLogger& logger, CHistorian* historian,
                                         CCursorContextAnalyzer* cursor_analyzer)
    : m_logger(logger)
    , m_historian(historian)
    , m_cursor_analyzer(cursor_analyzer)
{
}

SOrchestrationResult CRequestOrchestrator::run(const SOrchestrationRequest& request) const
{
    const auto started = std::chrono::steady_clock::now();
    SOrchestrationResult result;
    result.m_route = "NO_APPLICABLE_CONTEXT";

    m_logger.log(ELogLevel::Info, SLogEvent::request_started(
        request.m_request_id, request.m_session_id, request.m_text.size(),
        !request.m_cursor_file.empty() && request.m_cursor_line > 0
            && request.m_cursor_column > 0,
        request.m_history.size()));

    if (request.m_text.empty())
    {
        result.m_status = "REJECTED";
        result.m_exit_reason = "EMPTY_REQUEST";
        result.m_message = "Request rejected: text is empty.";
        result.m_trace = {"VALIDATION:FAILED", "EXIT:EMPTY_REQUEST"};
        m_logger.log(ELogLevel::Error, SLogEvent::request_finished(
            request.m_request_id, request.m_session_id, result.m_status,
            result.m_route, result.m_exit_reason, 0));
        return result;
    }

    result.m_trace.emplace_back("VALIDATION:OK");
    bool cursor_applicable = false;
    bool history_applicable = false;
    std::string failure;

    const bool has_cursor = !request.m_cursor_file.empty()
        && request.m_cursor_line > 0 && request.m_cursor_column > 0;

    if (has_cursor && m_cursor_analyzer != nullptr)
    {
        const SCursorContextResult cursor = m_cursor_analyzer->assess(
            request.m_text, request.m_cursor_file, request.m_cursor_line,
            request.m_cursor_column, request.m_cursor_compile_arguments);
        result.m_cursor_symbol = cursor.m_symbol;
        m_logger.log(cursor.m_ok ? ELogLevel::Info : ELogLevel::Error,
            SLogEvent::cursor_assessed(request.m_request_id, cursor.m_symbol.has_value(),
                cursor.m_llm_called, cursor.m_ok, cursor.m_context_applicable,
                cursor.m_escalated));
        if (!cursor.m_ok)
            failure = "CURSOR_RELEVANCE_FAILED";
        else
            cursor_applicable = cursor.m_context_applicable;
    }

    // Assess history independently: cursor and history can both be applicable.
    if (failure.empty() && !request.m_history.empty() && m_historian != nullptr)
    {
        const SHistoryRelevance history =
            m_historian->assess_relevance(request.m_text, request.m_history);
        m_logger.log(history.m_ok ? ELogLevel::Info : ELogLevel::Error,
            SLogEvent::history_assessed(request.m_request_id, request.m_history.size(),
                history.m_ok, history.m_history_needed, history.m_escalated));
        if (!history.m_ok)
            failure = "HISTORY_RELEVANCE_FAILED";
        else
            history_applicable = history.m_history_needed;
    }

    if (!failure.empty())
    {
        result.m_ok = false;
        result.m_status = "FAILED";
        result.m_exit_reason = failure;
        result.m_message = failure;
        result.m_trace.emplace_back("APPLICABILITY:FAILED");
    }
    else
    {
        if (cursor_applicable && history_applicable)
            result.m_route = "CURSOR_AND_HISTORY_CONTEXT_REQUIRED";
        else if (cursor_applicable)
            result.m_route = "CURSOR_CONTEXT_REQUIRED";
        else if (history_applicable)
            result.m_route = "HISTORY_CONTEXT_REQUIRED";
        else
            result.m_route = "NO_APPLICABLE_CONTEXT";

        result.m_ok = true;
        result.m_status = "STUB_COMPLETE";
        result.m_exit_reason = "SCOPE_CLASSIFICATION_NOT_IMPLEMENTED";
        result.m_message = "No answer generated: scope classification is not implemented yet.";
        result.m_trace.emplace_back("APPLICABILITY:ASSESSED");
        result.m_trace.emplace_back(result.m_route);
        m_logger.log(ELogLevel::Info,
            SLogEvent::route_selected(request.m_request_id, result.m_route));
        result.m_trace.emplace_back("SCOPE:NOT_IMPLEMENTED");
    }

    result.m_trace.emplace_back("EXIT:" + result.m_exit_reason);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    m_logger.log(result.m_ok ? ELogLevel::Info : ELogLevel::Error,
        SLogEvent::request_finished(request.m_request_id, request.m_session_id,
            result.m_status, result.m_route, result.m_exit_reason, elapsed));
    return result;
}

} // namespace fcpp

