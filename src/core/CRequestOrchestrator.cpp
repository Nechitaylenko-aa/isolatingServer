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
    result.m_route = EOrchestrationRoute::NoApplicableContext;

    m_logger.log(ELogLevel::Info, request);

    if (request.m_text.empty())
    {
        result.m_status = ERequestStatus::Rejected;
        result.m_exit_reason = EExitReason::EmptyRequest;
        result.m_message = "Request rejected: text is empty.";
        result.m_trace = {ETraceEvent::ValidationFailed, ETraceEvent::ExitEmptyRequest};
        m_logger.log(ELogLevel::Error, result, request.m_request_id,
            request.m_session_id, 0);
        return result;
    }

    result.m_trace.emplace_back(ETraceEvent::ValidationOk);
    bool cursor_applicable = false;
    bool history_applicable = false;
    EExitReason failure = EExitReason::None;

    const bool has_cursor = !request.m_cursor_file.empty()
        && request.m_cursor_line > 0 && request.m_cursor_column > 0;

    if (has_cursor && m_cursor_analyzer != nullptr)
    {
        const SCursorContextResult cursor = m_cursor_analyzer->assess(
            request.m_text, request.m_cursor_file, request.m_cursor_line,
            request.m_cursor_column, request.m_cursor_compile_arguments);
        result.m_cursor_symbol = cursor.m_symbol;
        m_logger.log(cursor.m_ok ? ELogLevel::Info : ELogLevel::Error,
            cursor, request.m_request_id);
        if (!cursor.m_ok)
            failure = EExitReason::CursorRelevanceFailed;
        else
            cursor_applicable = cursor.m_context_applicable;
    }

    // Assess history independently: cursor and history can both be applicable.
    if (failure.empty() && !request.m_history.empty() && m_historian != nullptr)
    {
        const SHistoryRelevance history =
            m_historian->assess_relevance(request.m_text, request.m_history);
        m_logger.log(history.m_ok ? ELogLevel::Info : ELogLevel::Error,
            history, request.m_request_id, request.m_history.size());
        if (!history.m_ok)
            failure = EExitReason::HistoryRelevanceFailed;
        else
            history_applicable = history.m_history_needed;
    }

    if (failure != EExitReason::None)
    {
        result.m_ok = false;
        result.m_status = ERequestStatus::Failed;
        result.m_exit_reason = failure;
        result.m_message = exit_reason_name(failure);
        result.m_trace.emplace_back(ETraceEvent::ApplicabilityFailed);
    }
    else
    {
        if (cursor_applicable && history_applicable)
            result.m_route = EOrchestrationRoute::CursorAndHistoryContextRequired;
        else if (cursor_applicable)
            result.m_route = EOrchestrationRoute::CursorContextRequired;
        else if (history_applicable)
            result.m_route = EOrchestrationRoute::HistoryContextRequired;
        else
            result.m_route = EOrchestrationRoute::NoApplicableContext;

        result.m_ok = true;
        result.m_status = ERequestStatus::StubComplete;
        result.m_exit_reason = EExitReason::ScopeClassificationNotImplemented;
        result.m_message = "No answer generated: scope classification is not implemented yet.";
        result.m_trace.emplace_back(ETraceEvent::ApplicabilityAssessed);
        result.m_trace.emplace_back(ETraceEvent::RouteSelected);
        m_logger.log(ELogLevel::Info,
            SLogEvent::route_selected(request.m_request_id, orchestration_route_name(result.m_route)));
        result.m_trace.emplace_back(ETraceEvent::ScopeNotImplemented);
    }

    switch (result.m_exit_reason)
    {
    case EExitReason::EmptyRequest:
        result.m_trace.emplace_back(ETraceEvent::ExitEmptyRequest);
        break;
    case EExitReason::CursorRelevanceFailed:
        result.m_trace.emplace_back(ETraceEvent::ExitCursorRelevanceFailed);
        break;
    case EExitReason::HistoryRelevanceFailed:
        result.m_trace.emplace_back(ETraceEvent::ExitHistoryRelevanceFailed);
        break;
    case EExitReason::ScopeClassificationNotImplemented:
        result.m_trace.emplace_back(ETraceEvent::ExitScopeClassificationNotImplemented);
        break;
    case EExitReason::None:
        result.m_trace.emplace_back(ETraceEvent::ExitCompleted);
        break;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    m_logger.log(result.m_ok ? ELogLevel::Info : ELogLevel::Error,
        result, request.m_request_id, request.m_session_id, elapsed);
    return result;
}

} // namespace fcpp

