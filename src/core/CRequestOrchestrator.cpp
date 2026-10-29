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
namespace
{

enum class EApplicability
{
    Failed,
    NotApplicable,
    Applicable
};

} // namespace

CRequestOrchestrator::CRequestOrchestrator(CLogger& logger, CHistorian* historian,
                                         CCursorContextAnalyzer* cursor_analyzer, CLlmLayer* llm)
    : m_logger(logger)
    , m_historian(historian)
    , m_cursor_analyzer(cursor_analyzer)
    , m_llm(llm)
{
}

void CRequestOrchestrator::generate_and_audit_default_answer(
    const SOrchestrationRequest& request, SOrchestrationResult& result) const
{
    if (m_llm == nullptr)
    {
        result.m_ok = false;
        result.m_status = ERequestStatus::Failed;
        result.m_exit_reason = EExitReason::LlmGenerationFailed;
        result.m_message = "LLM layer is not configured.";
        result.m_trace.emplace_back(ETraceEvent::ExitLlmGenerationFailed);
        m_logger.log_event(ELogLevel::Error, "answer.generation_failed",
            {{"request_id", request.m_request_id}, {"reason", result.m_message}});
        return;
    }

    const SLlmExecutionResult generated = m_llm->request_text(
        ELlmRole::CoderLocal, EPromptType::CppAssistant, request.m_text);
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
        return;
    }

    result.m_trace.emplace_back(ETraceEvent::AnswerGenerated);
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
        return;
    }

    result.m_ok = true;
    result.m_status = ERequestStatus::Completed;
    result.m_exit_reason = EExitReason::None;
    result.m_message = generated.m_text;
    result.m_trace.emplace_back(ETraceEvent::AnswerAudited);
}

CRequestOrchestrator::ERootChoice CRequestOrchestrator::choose_root(
    const SOrchestrationRequest& request, SOrchestrationResult& result) const
{
    EApplicability cursor = EApplicability::NotApplicable;
    EApplicability history = EApplicability::NotApplicable;
    EExitReason failure = EExitReason::None;

    const bool cursor_position_provided = !request.m_cursor_file.empty()
        && request.m_cursor_line > 0 && request.m_cursor_column > 0;

    if (cursor_position_provided && m_cursor_analyzer != nullptr)
    {
        const SCursorContextResult assessed = m_cursor_analyzer->assess(
            request.m_text, request.m_cursor_file, request.m_cursor_line,
            request.m_cursor_column, request.m_cursor_compile_arguments);
        result.m_cursor_symbol = assessed.m_symbol;
        m_logger.log(assessed.m_ok ? ELogLevel::Info : ELogLevel::Error,
            assessed, request.m_request_id);

        if (!assessed.m_ok)
            failure = EExitReason::CursorRelevanceFailed;
        else
            cursor = assessed.m_context_applicable
                ? EApplicability::Applicable : EApplicability::NotApplicable;
    }

    // History is assessed independently: both contexts may be applicable.
    if (failure == EExitReason::None
        && !request.m_history.empty() && m_historian != nullptr)
    {
        const SHistoryRelevance assessed =
            m_historian->assess_relevance(request.m_text, request.m_history);
        m_logger.log(assessed.m_ok ? ELogLevel::Info : ELogLevel::Error,
            assessed, request.m_request_id, request.m_history.size());

        if (!assessed.m_ok)
            failure = EExitReason::HistoryRelevanceFailed;
        else
            history = assessed.m_history_needed
                ? EApplicability::Applicable : EApplicability::NotApplicable;
    }

    if (failure != EExitReason::None)
    {
        result.m_ok = false;
        result.m_status = ERequestStatus::Failed;
        result.m_exit_reason = failure;
        result.m_message = exit_reason_name(failure);
        result.m_trace.emplace_back(ETraceEvent::ApplicabilityFailed);
        return ERootChoice::Failure;
    }

    result.m_trace.emplace_back(ETraceEvent::ApplicabilityAssessed);
    return cursor == EApplicability::Applicable
        ? (history == EApplicability::Applicable
            ? ERootChoice::CursorAndHistory : ERootChoice::Cursor)
        : (history == EApplicability::Applicable
            ? ERootChoice::History : ERootChoice::NoContext);
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

    switch (choose_root(request, result))
    {
    case ERootChoice::Failure:
        break;

    case ERootChoice::CursorAndHistory:
        result.m_route = EOrchestrationRoute::CursorAndHistoryContextRequired;
        break;

    case ERootChoice::Cursor:
        result.m_route = EOrchestrationRoute::CursorContextRequired;
        break;

    case ERootChoice::History:
        result.m_route = EOrchestrationRoute::HistoryContextRequired;
        break;

    case ERootChoice::NoContext:
        result.m_route = EOrchestrationRoute::NoApplicableContext;
        break;
    }

    if (result.m_exit_reason == EExitReason::None)
    {
        result.m_trace.emplace_back(ETraceEvent::RouteSelected);
        m_logger.log(ELogLevel::Info,
            SLogEvent::route_selected(request.m_request_id, result.m_route));

        switch (result.m_route)
        {
        case EOrchestrationRoute::CursorAndHistoryContextRequired:
        case EOrchestrationRoute::CursorContextRequired:
        case EOrchestrationRoute::HistoryContextRequired:
            result.m_ok = true;
            result.m_status = ERequestStatus::StubComplete;
            result.m_exit_reason = EExitReason::ScopeClassificationNotImplemented;
            result.m_message = "Selected context route is not implemented yet.";
            result.m_trace.emplace_back(ETraceEvent::ScopeNotImplemented);
            break;

        case EOrchestrationRoute::NoApplicableContext:
            generate_and_audit_default_answer(request, result);
            break;
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
    case EExitReason::LlmGenerationFailed:
        // The specific terminal event was recorded at the failure site.
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
