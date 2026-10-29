/**
 * @file CRequestOrchestrator.cpp
 * @brief Реализация дерева обработки запроса.
 */

#include "core/CRequestOrchestrator.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <utility>

#include "core/CHistorian.h"
#include "core/CCursorContextAnalyzer.h"
#include "core/CLogger.h"
#include "fcpp-agent/IFactProvider.h"
#include "fcpp-agent/SLogEvent.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{
namespace
{

/**
 * @brief Убрать пробельные символы по краям.
 * @param text Строка.
 * @return Строка без ведущих и замыкающих пробелов.
 */
std::string trim(const std::string& text)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string::npos)
    {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

} // namespace

CRequestOrchestrator::CRequestOrchestrator(CLogger& logger, CHistorian* historian,
                                           CCursorContextAnalyzer* cursor_analyzer,
                                           CLlmLayer* llm, const IFactProvider* facts)
    : m_logger(logger)
    , m_historian(historian)
    , m_cursor_analyzer(cursor_analyzer)
    , m_llm(llm)
    , m_facts(facts)
{
}

void CRequestOrchestrator::fail(const SOrchestrationRequest& request,
                                SOrchestrationResult& result,
                                EExitReason reason,
                                const std::string& message,
                                ETraceEvent event) const
{
    result.m_ok = false;
    result.m_status = ERequestStatus::Failed;
    result.m_exit_reason = reason;
    result.m_message = message;
    result.m_trace.emplace_back(event);
    m_logger.log_event(ELogLevel::Error, "orchestrator.failed",
        {{"request_id", request.m_request_id},
         {"route", orchestration_route_name(result.m_route)},
         {"exit_reason", exit_reason_name(reason)},
         {"reason", message}});
}

bool CRequestOrchestrator::normalise(SOrchestrationRequest& request) const
{
    const std::string trimmed = trim(request.m_text);
    if (trimmed.empty())
    {
        return false;
    }
    request.m_text = trimmed;
    return true;
}

bool CRequestOrchestrator::collect_facts(const SOrchestrationRequest& request,
                                         SOrchestrationResult& result) const
{
    if (m_facts == nullptr || !result.m_cursor_symbol.has_value())
    {
        // Спрашивать не у кого либо не о чем. Это не сбой.
        return true;
    }

    SFactRequest fact_request;
    fact_request.m_qualified_name = result.m_cursor_symbol->m_qualified_name;
    fact_request.m_name = result.m_cursor_symbol->m_name;
    fact_request.m_file = result.m_cursor_symbol->m_location.m_file;
    fact_request.m_line = result.m_cursor_symbol->m_location.m_line;

    const SFactResult facts = m_facts->collect(fact_request);
    if (!facts.m_ok)
    {
        fail(request, result, EExitReason::IndexUnavailable,
             "Project index is unavailable: " + facts.m_error,
             ETraceEvent::ExitIndexUnavailable);
        return false;
    }

    result.m_fact_sections = facts.m_sections;
    m_logger.log_event(ELogLevel::Info, "facts.collected",
        {{"request_id", request.m_request_id},
         {"entity", fact_request.m_qualified_name},
         {"sections", facts.m_sections.size()}});
    return true;
}

std::string CRequestOrchestrator::build_evidence_block(
    const SOrchestrationResult& result) const
{
    std::string block;
    if (!result.m_cursor_context.empty())
    {
        block += "CURSOR SOURCE CONTEXT (extracted by Clang):\n"
            + result.m_cursor_context;
    }
    for (const SFactSection& section : result.m_fact_sections)
    {
        if (!block.empty())
        {
            block += "\n\n";
        }
        block += "PROJECT INDEX FACTS [" + section.m_title + "]:\n" + section.m_text;
    }
    return block;
}

bool CRequestOrchestrator::collect_file_entity_facts(
    const SOrchestrationRequest& request, SOrchestrationResult& result) const
{
    if (m_facts == nullptr || m_cursor_analyzer == nullptr)
    {
        return false;
    }

    const SFileEntityChoice choice = m_cursor_analyzer->choose_file_entity(
        request.m_text, request.m_cursor_file, result.m_file_context);
    m_logger.log_event(choice.m_ok ? ELogLevel::Info : ELogLevel::Warn,
        "file.entity_choice",
        {{"request_id", request.m_request_id},
         {"ok", choice.m_ok},
         {"single", choice.m_single},
         {"entity", choice.m_qualified_name},
         {"error", choice.m_error}});

    if (!choice.m_ok || !choice.m_single)
    {
        // No single entity, or the model failed. Answering from the whole file
        // is correct here; inventing an entity is not.
        return false;
    }

    SFactRequest fact_request;
    fact_request.m_qualified_name = choice.m_qualified_name;
    fact_request.m_file = request.m_cursor_file;
    fact_request.m_expected_file = request.m_cursor_file;

    const SFactResult facts = m_facts->collect(fact_request);
    if (!facts.m_ok)
    {
        // Index failure is a visible event, but the file content is still
        // available, so the request can be answered without the index facts.
        m_logger.log_event(ELogLevel::Warn, "facts.index_unavailable_for_file",
            {{"request_id", request.m_request_id},
             {"reason", facts.m_error}});
        return false;
    }
    if (facts.m_rejected)
    {
        // The model named an entity that is not declared in this file.
        m_logger.log_event(ELogLevel::Warn, "file.entity_rejected",
            {{"request_id", request.m_request_id},
             {"entity", choice.m_qualified_name},
             {"reason", facts.m_error}});
        return false;
    }

    result.m_file_entity = choice.m_qualified_name;
    result.m_fact_sections = facts.m_sections;
    return true;
}

void CRequestOrchestrator::answer_from_file(const SOrchestrationRequest& request,
                                            SOrchestrationResult& result,
                                            bool include_history) const
{
    std::string prompt;
    if (!result.m_fact_sections.empty())
    {
        // Confirmed index facts are the evidence. The raw file is not attached:
        // duplicating facts with the whole source violates Zero-Window and
        // doubles the prompt for no information gain.
        prompt = "PROJECT INDEX FACTS FOR " + result.m_file_entity + ":\n"
            + build_evidence_block(result);
        m_logger.log_event(ELogLevel::Info, "prompt.composed",
            {{"request_id", request.m_request_id},
             {"stage", "file_answer"},
             {"mode", "facts_only"},
             {"entity", result.m_file_entity},
             {"prompt_bytes", prompt.size()}});
    }
    else
    {
        // No confirmed entity: a bounded slice, not the whole file. The same
        // limits as the cursor path keep the context window predictable.
        SFileSliceLimits limits;
        std::string slice;
        bool truncated = false;
        std::string error;
        if (read_file_slice(request.m_cursor_file, limits, slice, truncated, error))
        {
            prompt = "FILE CONTENT (bounded slice):\n" + slice;
        }
        else
        {
            // The slice was already read during relevance assessment; reuse it
            // rather than failing after the file was successfully opened once.
            prompt = "FILE CONTENT:\n" + result.m_file_context;
        }
        m_logger.log_event(ELogLevel::Info, "prompt.composed",
            {{"request_id", request.m_request_id},
             {"stage", "file_answer"},
             {"mode", "bounded_slice"},
             {"truncated", truncated},
             {"prompt_bytes", prompt.size()}});
    }

    if (include_history && !result.m_history_context.empty())
    {
        prompt += "\n\nRELEVANT CONVERSATION HISTORY (selected by historian):\n";
        for (const SConversationTurn& turn : result.m_history_context)
        {
            prompt += "USER: " + turn.m_user_text + "\n";
            prompt += "ASSISTANT: " + turn.m_assistant_text + "\n\n";
        }
    }
    prompt += "\nUSER REQUEST:\n" + request.m_text;

    generate_answer(request, result, ELlmRole::CoderSmart,
                    EPromptType::CppExpert, prompt);
}

void CRequestOrchestrator::generate_answer(const SOrchestrationRequest& request,
                                           SOrchestrationResult& result,
                                           ELlmRole role,
                                           EPromptType prompt_type,
                                           const std::string& prompt_input) const
{
    if (m_llm == nullptr)
    {
        fail(request, result, EExitReason::LlmGenerationFailed,
             "LLM layer is not configured.", ETraceEvent::ExitLlmGenerationFailed);
        return;
    }

    const SLlmExecutionResult generated =
        m_llm->request_text(role, prompt_type, prompt_input);
    if (!generated.m_ok)
    {
        fail(request, result, EExitReason::LlmGenerationFailed,
             generated.m_error.empty() ? "LLM answer generation failed." : generated.m_error,
             ETraceEvent::ExitLlmGenerationFailed);
        return;
    }

    result.m_trace.emplace_back(ETraceEvent::AnswerGenerated);
    m_logger.log_event(ELogLevel::Info, "answer.audit",
        {{"request_id", request.m_request_id},
         {"accepted", true},
         {"answer_bytes", generated.m_text.size()},
         {"role", llm_role_name(generated.m_used_role)},
         {"escalated", generated.m_escalated}});

    result.m_ok = true;
    result.m_status = ERequestStatus::Completed;
    result.m_exit_reason = EExitReason::None;
    result.m_message = generated.m_text;
    result.m_trace.emplace_back(ETraceEvent::AnswerAudited);
}

CRequestOrchestrator::ERootChoice CRequestOrchestrator::choose_root(
    const SOrchestrationRequest& request, SOrchestrationResult& result) const
{
    bool cursor_applicable = false;
    bool history_applicable = false;
    bool file_applicable = false;
    EExitReason first_failure = EExitReason::None;

    // A file may arrive without a cursor position. That is a distinct case, not
    // a missing cursor: the editor points at a file the user is talking about.
    const bool has_position = request.m_cursor_line > 0 && request.m_cursor_column > 0;

    if (!request.m_cursor_file.empty() && !has_position
        && m_cursor_analyzer != nullptr)
    {
        const SFileContextResult assessed =
            m_cursor_analyzer->assess_file(request.m_text, request.m_cursor_file);
        m_logger.log_event(assessed.m_ok ? ELogLevel::Info : ELogLevel::Error,
            "file.assessed",
            {{"request_id", request.m_request_id},
             {"ok", assessed.m_ok},
             {"applicable", assessed.m_context_applicable},
             {"llm_called", assessed.m_llm_called},
             {"truncated", assessed.m_truncated},
             {"bytes", assessed.m_content.size()},
             {"error", assessed.m_error}});

        if (assessed.m_ok)
        {
            file_applicable = assessed.m_context_applicable;
            result.m_file_context = assessed.m_content;
        }
        else
        {
            first_failure = EExitReason::FileRelevanceFailed;
        }
    }

    // Cursor and history are assessed independently. A failure of one source
    // must not suppress the other: the log shows both outcomes.
    if (!request.m_cursor_file.empty() && has_position
        && m_cursor_analyzer != nullptr)
    {
        const SCursorContextResult assessed = m_cursor_analyzer->assess(
            request.m_text, request.m_cursor_file, request.m_cursor_line,
            request.m_cursor_column, request.m_cursor_compile_arguments);
        result.m_cursor_symbol = assessed.m_symbol;
        result.m_cursor_context = assessed.m_source_context;
        m_logger.log(assessed.m_ok ? ELogLevel::Info : ELogLevel::Error,
                     assessed, request.m_request_id);

        if (assessed.m_ok)
        {
            cursor_applicable = assessed.m_context_applicable;
        }
        else
        {
            first_failure = EExitReason::CursorRelevanceFailed;
        }
    }

    if (!request.m_history.empty() && m_historian != nullptr)
    {
        const SHistoryRelevance assessed =
            m_historian->assess_relevance(request.m_text, request.m_history);
        m_logger.log(assessed.m_ok ? ELogLevel::Info : ELogLevel::Error,
                     assessed, request.m_request_id, request.m_history.size());

        if (assessed.m_ok)
        {
            history_applicable = assessed.m_history_needed;
            result.m_history_context.clear();
            for (const std::size_t index : assessed.m_relevant_turn_indices)
            {
                if (index < request.m_history.size())
                {
                    result.m_history_context.push_back(request.m_history[index]);
                }
            }
        }
        else if (first_failure == EExitReason::None)
        {
            first_failure = EExitReason::HistoryRelevanceFailed;
        }
    }

    if (first_failure != EExitReason::None)
    {
        return ERootChoice::Failure;
    }
    result.m_trace.emplace_back(ETraceEvent::ApplicabilityAssessed);

    const bool is_cursor = cursor_applicable;
    const bool is_file = file_applicable;
    const bool is_history = history_applicable;

    // Six routes, six mutually exclusive conditions over three flags: at most
    // one condition can hold, so the order below decides nothing.
    if (is_cursor && is_history)
    {
        return ERootChoice::CursorAndHistory;
    }
    if (is_cursor && !is_history)
    {
        return ERootChoice::Cursor;
    }
    if (!is_cursor && is_file && is_history)
    {
        return ERootChoice::FileAndHistory;
    }
    if (!is_cursor && is_file && !is_history)
    {
        return ERootChoice::File;
    }
    if (!is_cursor && !is_file && is_history)
    {
        return ERootChoice::History;
    }
    return ERootChoice::NoContext;
}

SOrchestrationResult CRequestOrchestrator::run(const SOrchestrationRequest& request) const
{
    const auto started = std::chrono::steady_clock::now();
    SOrchestrationResult result;
    SOrchestrationRequest normalised = request;

    m_logger.log(ELogLevel::Info, normalised);

    if (!normalise(normalised))
    {
        result.m_status = ERequestStatus::Rejected;
        result.m_exit_reason = EExitReason::EmptyRequest;
        result.m_message = "Request rejected: text is empty after normalisation.";
        result.m_trace = {ETraceEvent::ValidationFailed, ETraceEvent::ExitEmptyRequest};
        m_logger.log(ELogLevel::Error, result, normalised.m_request_id,
                     normalised.m_session_id, 0);
        return result;
    }
    result.m_trace.emplace_back(ETraceEvent::ValidationOk);

    switch (choose_root(normalised, result))
    {
    case ERootChoice::Failure:
        fail(normalised, result, result.m_exit_reason == EExitReason::None
                 ? EExitReason::CursorRelevanceFailed : result.m_exit_reason,
             "Context applicability assessment failed.", ETraceEvent::ApplicabilityFailed);
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
    case ERootChoice::File:
        result.m_route = EOrchestrationRoute::FileContextRequired;
        break;
    case ERootChoice::FileAndHistory:
        result.m_route = EOrchestrationRoute::FileAndHistoryContextRequired;
        break;
    case ERootChoice::NoContext:
        result.m_route = EOrchestrationRoute::NoApplicableContext;
        break;
    }

    // Every terminal path below reaches the single exit point, so the final
    // request.finished record is never skipped.
    if (result.m_exit_reason == EExitReason::None
        && result.m_route != EOrchestrationRoute::Unselected)
    {
        result.m_trace.emplace_back(ETraceEvent::RouteSelected);
        m_logger.log(ELogLevel::Info,
                     SLogEvent::route_selected(normalised.m_request_id, result.m_route));

        const bool needs_facts =
            result.m_route == EOrchestrationRoute::CursorContextRequired
            || result.m_route == EOrchestrationRoute::CursorAndHistoryContextRequired;

        if (!needs_facts || collect_facts(normalised, result))
        {
            switch (result.m_route)
            {
            case EOrchestrationRoute::CursorContextRequired:
            {
                if (result.m_cursor_context.empty())
                {
                    fail(normalised, result, EExitReason::CursorRelevanceFailed,
                         "Cursor route selected without source context.",
                         ETraceEvent::ExitCursorRelevanceFailed);
                    break;
                }
                std::string prompt = build_evidence_block(result);
                prompt += "\nUSER REQUEST:\n" + normalised.m_text;
                generate_answer(normalised, result, ELlmRole::CoderSmart,
                                EPromptType::CppExpert, prompt);
                break;
            }

            case EOrchestrationRoute::CursorAndHistoryContextRequired:
            {
                if (result.m_cursor_context.empty())
                {
                    fail(normalised, result, EExitReason::CursorRelevanceFailed,
                         "Cursor route selected without source context.",
                         ETraceEvent::ExitCursorRelevanceFailed);
                    break;
                }
                std::string prompt = build_evidence_block(result);
                prompt += "\n\nRELEVANT CONVERSATION HISTORY (selected by historian):\n";
                for (const SConversationTurn& turn : result.m_history_context)
                {
                    prompt += "USER: " + turn.m_user_text + "\n";
                    prompt += "ASSISTANT: " + turn.m_assistant_text + "\n\n";
                }
                prompt += "\nUSER REQUEST:\n" + normalised.m_text;
                generate_answer(normalised, result, ELlmRole::CoderSmart,
                                EPromptType::CppExpert, prompt);
                break;
            }

            case EOrchestrationRoute::HistoryContextRequired:
            {
                if (result.m_history_context.empty())
                {
                    fail(normalised, result, EExitReason::HistoryRelevanceFailed,
                         "History route selected without relevant conversation turns.",
                         ETraceEvent::ExitHistoryRelevanceFailed);
                    break;
                }
                std::string prompt =
                    "RELEVANT CONVERSATION HISTORY (selected by historian):\n";
                for (const SConversationTurn& turn : result.m_history_context)
                {
                    prompt += "USER: " + turn.m_user_text + "\n";
                    prompt += "ASSISTANT: " + turn.m_assistant_text + "\n\n";
                }
                prompt += "CURRENT USER REQUEST:\n" + normalised.m_text;
                generate_answer(normalised, result, ELlmRole::CoderLocal,
                                EPromptType::ConversationAssistant, prompt);
                break;
            }

            case EOrchestrationRoute::FileContextRequired:
            {
                if (result.m_file_context.empty())
                {
                    fail(normalised, result, EExitReason::FileNotRelevant,
                         "The open file does not relate to the request.",
                         ETraceEvent::ExitFileNotRelevant);
                    break;
                }
                // Narrow path: the entity is named by the model and confirmed by
                // the index. Wide path: the whole file goes to the answer model.
                (void)collect_file_entity_facts(normalised, result);
                answer_from_file(normalised, result, false);
                break;
            }

            case EOrchestrationRoute::FileAndHistoryContextRequired:
            {
                if (result.m_file_context.empty())
                {
                    fail(normalised, result, EExitReason::FileNotRelevant,
                         "The open file does not relate to the request.",
                         ETraceEvent::ExitFileNotRelevant);
                    break;
                }
                if (result.m_history_context.empty())
                {
                    fail(normalised, result, EExitReason::HistoryRelevanceFailed,
                         "File and history route selected without relevant turns.",
                         ETraceEvent::ExitHistoryRelevanceFailed);
                    break;
                }
                (void)collect_file_entity_facts(normalised, result);
                answer_from_file(normalised, result, true);
                break;
            }

            case EOrchestrationRoute::NoApplicableContext:
                generate_answer(normalised, result, ELlmRole::CoderLocal,
                                EPromptType::CppAssistant, normalised.m_text);
                break;

            case EOrchestrationRoute::Unselected:
            case EOrchestrationRoute::Count:
                break;
            }
        }
    }

    // Terminal trace event: exactly one, unless the failure site already
    // recorded the matching one.
    ETraceEvent terminal = ETraceEvent::ExitCompleted;
    switch (result.m_exit_reason)
    {
    case EExitReason::EmptyRequest:
        terminal = ETraceEvent::ExitEmptyRequest;
        break;
    case EExitReason::CursorRelevanceFailed:
        terminal = ETraceEvent::ExitCursorRelevanceFailed;
        break;
    case EExitReason::HistoryRelevanceFailed:
        terminal = ETraceEvent::ExitHistoryRelevanceFailed;
        break;
    case EExitReason::FileRelevanceFailed:
        terminal = ETraceEvent::ExitFileRelevanceFailed;
        break;
    case EExitReason::FileNotRelevant:
        terminal = ETraceEvent::ExitFileNotRelevant;
        break;
    case EExitReason::IndexUnavailable:
        terminal = ETraceEvent::ExitIndexUnavailable;
        break;
    case EExitReason::LlmGenerationFailed:
        terminal = ETraceEvent::ExitLlmGenerationFailed;
        break;
    case EExitReason::None:
        terminal = ETraceEvent::ExitCompleted;
        break;
    case EExitReason::Count:
        break;
    }
    if (result.m_trace.empty() || result.m_trace.back() != terminal)
    {
        result.m_trace.emplace_back(terminal);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    m_logger.log(result.m_ok ? ELogLevel::Info : ELogLevel::Error,
                 result, normalised.m_request_id, normalised.m_session_id, elapsed);
    return result;
}

} // namespace fcpp
