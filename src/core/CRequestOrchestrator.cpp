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
    using TClock = std::chrono::steady_clock;
    const auto started = TClock::now();

    SOrchestrationResult result;
    result.m_route = "UNDECIDED";

    const auto log_event = [this, &request](ELogLevel level,
                                            const std::string& event,
                                            const nlohmann::json& fields)
    {
        nlohmann::json payload = fields;
        payload["request_id"] = request.m_request_id;
        payload["session_id"] = request.m_session_id;
        m_logger.log_event(level, event, payload);
    };

    const auto stage = [&result, &log_event](const std::string& name,
                                              const std::string& outcome,
                                              const nlohmann::json& details = nlohmann::json::object())
    {
        result.m_trace.push_back(name + ":" + outcome);
        nlohmann::json fields = details;
        fields["stage"] = name;
        fields["outcome"] = outcome;
        log_event(ELogLevel::Info, "orchestrator.stage", fields);
    };

    log_event(ELogLevel::Info, "orchestrator.started", {
        {"session_id", request.m_session_id},
        {"text_length", request.m_text.size()},
        {"query", request.m_text},
        {"has_cursor", !request.m_cursor_file.empty()
            && request.m_cursor_line > 0 && request.m_cursor_column > 0},
        {"history_turns", request.m_history.size()},
        {"project_open", request.m_project_open}
    });

    stage("REQUEST_RECEIVED", "OK");

    // Technical validation only. Semantic understanding belongs to the next stage.
    if (request.m_text.empty())
    {
        stage("TECHNICAL_VALIDATION", "FAILED", {{"reason", "empty_request"}});
        result.m_ok = false;
        result.m_status = "REJECTED";
        result.m_exit_reason = "EMPTY_REQUEST";
        result.m_message = "Request rejected: text is empty.";
    }
    else
    {
        stage("TECHNICAL_VALIDATION", "OK");

        // Placeholder: the real intelligibility analyzer will be plugged in here.
        stage("INTELLIGIBILITY_CHECK", "STUB_ACCEPTED",
              {{"implementation", "placeholder_non_empty_check"}});

        bool cursor_applicable = false;
        bool cursor_analysis_failed = false;
        std::string cursor_error;
        if (request.m_cursor_file.empty() || request.m_cursor_line == 0 || request.m_cursor_column == 0)
        {
            stage("CURSOR_APPLICABILITY", "NOT_APPLICABLE_NO_CURSOR");
        }
        else if (m_cursor_analyzer == nullptr)
        {
            stage("CURSOR_APPLICABILITY", "STUB_NO_ANALYZER_INJECTED",
                  {{"implementation", "cursor_analyzer_dependency_not_supplied"}});
        }
        else
        {
            const SCursorContextResult cursor = m_cursor_analyzer->assess(
                request.m_text, request.m_cursor_file, request.m_cursor_line,
                request.m_cursor_column, request.m_cursor_compile_arguments);
            result.m_cursor_symbol = cursor.m_symbol;
            cursor_applicable = cursor.m_ok && cursor.m_context_applicable;
            cursor_analysis_failed = !cursor.m_ok;
            cursor_error = cursor.m_error;
            const std::string outcome = !cursor.m_ok ? "FAILED"
                : (!cursor.m_symbol.has_value() ? "NOT_APPLICABLE_NO_SYMBOL"
                   : (cursor.m_context_applicable ? "APPLICABLE" : "NOT_APPLICABLE"));
            stage("CURSOR_APPLICABILITY", outcome,
                  {{"llm_called", cursor.m_llm_called},
                   {"used_role", llm_role_name(cursor.m_used_role)},
                   {"escalated", cursor.m_escalated},
                   {"reason", cursor.m_error}});
        }

        bool history_applicable = false;
        bool history_analysis_failed = false;
        std::string history_error;
        if (cursor_applicable)
        {
            stage("HISTORY_APPLICABILITY", "SKIPPED_CURSOR_CONTEXT_SELECTED");
        }
        else if (request.m_history.empty())
        {
            stage("HISTORY_APPLICABILITY", "NOT_APPLICABLE_NO_HISTORY");
        }
        else if (m_historian != nullptr)
        {
            const SHistoryRelevance relevance =
                m_historian->assess_relevance(request.m_text, request.m_history);
            history_applicable = relevance.m_ok && relevance.m_history_needed;
            history_analysis_failed = !relevance.m_ok;
            history_error = relevance.m_error;
            stage("HISTORY_APPLICABILITY",
                  relevance.m_ok ? (relevance.m_history_needed ? "APPLICABLE" : "NOT_APPLICABLE")
                                 : "FAILED",
                  {{"used_role", llm_role_name(relevance.m_used_role)},
                   {"escalated", relevance.m_escalated},
                   {"reason", relevance.m_error}});
        }
        else
        {
            stage("HISTORY_APPLICABILITY", "STUB_NO_HISTORIAN_INJECTED",
                  {{"implementation", "historian_dependency_not_supplied"}});
        }

        result.m_route = cursor_applicable ? "CURSOR_CONTEXT_REQUIRED"
            : (history_applicable ? "HISTORY_CONTEXT_REQUIRED" : "NO_APPLICABLE_CONTEXT");
        stage("CONTEXT_BRANCH", result.m_route);

        // Placeholder: general question vs. codebase question is not guessed here.
        stage("SCOPE_CLASSIFICATION", "STUB_NOT_IMPLEMENTED",
              {{"implementation", "pending_scope_analyzer"}});

        // Continue walking the declared tree so callers receive a complete,
        // explicit trace. These stages are skipped because scope is unresolved.
        stage("CONTEXT_DATA_COLLECTION", "SKIPPED_UPSTREAM_STUB");
        stage("CONTEXT_ASSEMBLY", "SKIPPED_UPSTREAM_STUB");
        stage("ANSWER_STRATEGY_SELECTION", "SKIPPED_UPSTREAM_STUB");
        stage("ANSWER_GENERATION", "SKIPPED_UPSTREAM_STUB");
        stage("FINAL_RESPONSE", "SERVICE_DIAGNOSTIC_ONLY");

        result.m_ok = true;
        result.m_status = "STUB_COMPLETE";
        result.m_exit_reason = "SCOPE_CLASSIFICATION_NOT_IMPLEMENTED";
        result.m_message = "No answer generated: scope classification is not implemented yet.";
        if (cursor_analysis_failed)
        {
            result.m_ok = false;
            result.m_status = "FAILED";
            result.m_exit_reason = "CURSOR_RELEVANCE_FAILED";
            result.m_message = "Cursor relevance assessment failed: " + cursor_error;
        }
        else if (history_analysis_failed)
        {
            result.m_ok = false;
            result.m_status = "FAILED";
            result.m_exit_reason = "HISTORY_RELEVANCE_FAILED";
            result.m_message = "History relevance assessment failed: " + history_error;
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        TClock::now() - started).count();
    result.m_trace.push_back("EXIT:" + result.m_exit_reason);
    const std::string detail_message = result.m_message;
    result.m_message =
        "ORCHESTRATION_STATUS=" + result.m_status
        + "\nROUTE=" + result.m_route
        + "\nEXIT_REASON=" + result.m_exit_reason
        + "\nTRACE=";
    for (std::size_t index = 0; index < result.m_trace.size(); ++index)
    {
        if (index != 0)
        {
            result.m_message += " -> ";
        }
        result.m_message += result.m_trace[index];
    }
    result.m_message += "\nDETAIL=" + detail_message;

    log_event(result.m_ok ? ELogLevel::Info : ELogLevel::Error,
              "orchestrator.finished",
              {
                  {"status", result.m_status},
                  {"route", result.m_route},
                  {"exit_reason", result.m_exit_reason},
                  {"elapsed_ms", elapsed},
                  {"trace", result.m_trace}
              });

    return result;
}

} // namespace fcpp

