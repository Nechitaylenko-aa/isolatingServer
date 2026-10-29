/**
 * @file CRequestOrchestrator.cpp
 * @brief Explicit, instrumented request-processing tree skeleton.
 */

#include "core/CRequestOrchestrator.h"
#include "core/CHistorian.h"
#include "core/CCursorContextAnalyzer.h"
#include "core/CLogger.h"
#include "fcpp-agent/SLogEvent.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{

CRequestOrchestrator::CRequestOrchestrator(CLogger& logger, CHistorian* historian,
                                         CCursorContextAnalyzer* cursor_analyzer, CLlmLayer* llm)
    : m_logger(logger)
    , m_historian(historian)
    , m_cursor_analyzer(cursor_analyzer)
    , m_llm(llm)
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
    if (failure == EExitReason::None && !request.m_history.empty() && m_historian != nullptr)
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

        result.m_trace.emplace_back(ETraceEvent::ApplicabilityAssessed);
        result.m_trace.emplace_back(ETraceEvent::RouteSelected);
        m_logger.log(ELogLevel::Info,
            SLogEvent::route_selected(request.m_request_id, result.m_route));

        if (result.m_route != EOrchestrationRoute::NoApplicableContext)
        {
            result.m_ok = true;
            result.m_status = ERequestStatus::StubComplete;
            result.m_exit_reason = EExitReason::ScopeClassificationNotImplemented;
            result.m_message = "Selected context route is not implemented yet.";
            result.m_trace.emplace_back(ETraceEvent::ScopeNotImplemented);
        }
        else if (m_llm == nullptr)
        {
            result.m_ok = false;
            result.m_status = ERequestStatus::Failed;
            result.m_exit_reason = EExitReason::LlmGenerationFailed;
            result.m_message = "LLM layer is not configured.";
            result.m_trace.emplace_back(ETraceEvent::ExitLlmGenerationFailed);
            m_logger.log_event(ELogLevel::Error, "answer.generation_failed",
                {{"request_id", request.m_request_id}, {"reason", result.m_message}});
        }
        else
        {
            constexpr const char* k_default_cpp_prompt =
                "You are a helpful, precise C++ software engineering assistant. "
                "Answer the user's question directly in the user's language. "
                "No repository, editor, or conversation context is provided. "
                "Do not claim to have inspected files or run commands. "
                "State assumptions when needed and do not invent project-specific facts.";
            const SLlmExecutionResult generated = m_llm->request_text(
                ELlmRole::CoderLocal, k_default_cpp_prompt, request.m_text);
            if (!generated.m_ok)
            {
                result.m_ok = false;
                result.m_status = ERequestStatus::Failed;
                result.m_exit_reason = EExitReason::LlmGenerationFailed;
                result.m_message = generated.m_error.empty()
                    ? "LLM answer generation failed." : generated.m_error;
                result.m_trace.emplace_back(ETraceEvent::ExitLlmGenerationFailed);
                m_logger.log_event(ELogLevel::Error, "answer.generation_failed",
                    {{"request_id", request.m_request_id}, {"reason", result.m_message},
                     {"role", llm_role_name(generated.m_used_role)},
                     {"escalated", generated.m_escalated}});
            }
            else
            {
                result.m_trace.emplace_back(ETraceEvent::AnswerGenerated);
                // Temporary audit stub: accept non-empty answers. Temperature/risk
                // scoring and independent review will be implemented behind this seam.
                const bool audit_ok = !generated.m_text.empty();
                m_logger.log_event(audit_ok ? ELogLevel::Info : ELogLevel::Error,
                    "answer.audit",
                    {{"request_id", request.m_request_id}, {"accepted", audit_ok},
                     {"audit", "stub"}, {"answer_bytes", generated.m_text.size()}});
                if (!audit_ok)
                {
                    result.m_ok = false;
                    result.m_status = ERequestStatus::Failed;
                    result.m_exit_reason = EExitReason::LlmGenerationFailed;
                    result.m_message = "Answer audit rejected an empty response.";
                    result.m_trace.emplace_back(ETraceEvent::ExitLlmGenerationFailed);
                }
                else
                {
                    result.m_ok = true;
                    result.m_status = ERequestStatus::Completed;
                    result.m_exit_reason = EExitReason::None;
                    result.m_message = generated.m_text;
                    result.m_trace.emplace_back(ETraceEvent::AnswerAudited);
                }
            }
        }
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

