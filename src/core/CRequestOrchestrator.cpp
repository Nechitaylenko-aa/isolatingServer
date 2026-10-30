/**
 * @file CRequestOrchestrator.cpp
 * @brief Реализация дерева обработки запроса.
 */

#include "core/CRequestOrchestrator.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "core/CHistorian.h"
#include "core/CCursorContextAnalyzer.h"
#include "core/CLogger.h"
#include "fcpp-agent/IFactProvider.h"
#include "fcpp-agent/ITool.h"
#include "fcpp-agent/SLogEvent.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{
namespace
{

/** @brief Оценка токенов по байтам (консервативная, как в LLM-слое). */
std::size_t estimate_tokens(const std::string& text)
{
    return (text.size() + 2U) / 3U + 4U;
}

/**
 * @brief Оставить не больше max_bytes байт с конца текста по границе UTF-8.
 *
 * Хвост, а не голова: при подгонке среза под чужое окно важнее конец блока,
 * где обычно находится вопрос, а не первые строки определения.
 *
 * @param text Исходный текст.
 * @param max_bytes Предел в байтах.
 * @return Усечённый текст по границе символа.
 */
std::string trim_utf8_tail(const std::string& text, std::size_t max_bytes)
{
    if (text.size() <= max_bytes)
    {
        return text;
    }
    std::size_t start = text.size() - max_bytes;
    // Do not start inside a multi-byte sequence.
    while (start < text.size()
           && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
    {
        ++start;
    }
    return "[CONTENT TRIMMED TO THE RESPONDING MODEL BUDGET]\n"
        + text.substr(start);
}

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

bool CRequestOrchestrator::check_request_fits(const SOrchestrationRequest& request,
                                             SOrchestrationResult& result) const
{
    if (m_llm == nullptr)
    {
        // No model means no window to reason about: the answer step reports the
        // missing layer itself, so this is not the place to fail.
        return true;
    }

    // The route is not chosen yet, so the answering role is unknown. A question
    // that fits the remote window may still not fit the local one, and the
    // reverse. Judging by a single role silently admits unanswerable requests.
    static const ELlmRole k_candidate_roles[]{
        ELlmRole::CoderSmart, ELlmRole::CoderLocal};

    const std::size_t request_tokens = estimate_tokens(request.m_text);
    std::size_t tightest_window = 0;
    SContextBudgetSplit tightest;

    for (const ELlmRole role : k_candidate_roles)
    {
        const std::size_t window = m_llm->context_window_tokens(role);
        const std::size_t reserve = m_llm->output_reserve_tokens(role);
        const SContextBudgetSplit split =
            split_context_budget(window, reserve, request_tokens);

        m_logger.log_event(ELogLevel::Info, "context.request_fit",
            {{"request_id", request.m_request_id},
             {"role", llm_role_name(role)},
             {"window_tokens", split.m_window},
             {"reserve_tokens", split.m_reserve},
             {"budget_tokens", split.m_total},
             {"request_tokens", split.m_request},
             {"fits", split.m_ok},
             {"error", split.m_error}});

        if (tightest_window == 0 || split.m_window < tightest_window)
        {
            tightest_window = split.m_window;
            tightest = split;
        }
        if (!split.m_ok)
        {
            // One refusing role is enough: the route may send the answer there.
            fail(request, result, EExitReason::RequestTooLong,
                 split.m_error, ETraceEvent::ExitRequestTooLong);
            return false;
        }
    }

    // Keep the tightest distribution as the pre-route diagnostic value. It is
    // replaced by the real role's budget once the route is known.
    result.m_budget = tightest;
    (void)tightest_window;
    return true;
}

void CRequestOrchestrator::plan_budget(ELlmRole role,
                                       const SOrchestrationRequest& request,
                                       SOrchestrationResult& result) const
{
    if (m_llm == nullptr || role == ELlmRole::Count)
    {
        return;
    }

    const std::size_t window = m_llm->context_window_tokens(role);
    const std::size_t reserve = m_llm->output_reserve_tokens(role);
    const std::size_t request_tokens = estimate_tokens(request.m_text);

    result.m_budget = split_context_budget(window, reserve, request_tokens);
    m_logger.log_event(ELogLevel::Info, "context.budget",
        {{"request_id", request.m_request_id},
         {"role", llm_role_name(role)},
         {"route", orchestration_route_name(result.m_route)},
         {"window_tokens", result.m_budget.m_window},
         {"reserve_tokens", result.m_budget.m_reserve},
         {"budget_tokens", result.m_budget.m_total},
         {"request_tokens", result.m_budget.m_request},
         {"cursor_limit_tokens", result.m_budget.m_cursor},
         {"history_limit_tokens", result.m_budget.m_history},
         {"ok", result.m_budget.m_ok},
         {"error", result.m_budget.m_error}});
}

bool CRequestOrchestrator::fit_material_to_budget(SOrchestrationResult& result) const
{
    if (!result.m_budget.m_ok)
    {
        return false;
    }

    // The material was sliced on assessment limits, which are deliberately
    // independent of the answering role. Fit it now that the role is known:
    // both operations are local edits, no model is called again.
    const std::size_t cursor_bytes = result.m_budget.m_cursor * 3U;
    const std::size_t history_bytes = result.m_budget.m_history * 3U;

    if (cursor_bytes > 0 && result.m_cursor_context.size() > cursor_bytes)
    {
        const std::string trimmed = trim_utf8_tail(result.m_cursor_context, cursor_bytes);
        m_logger.log_event(ELogLevel::Info, "context.cursor_trimmed",
            {{"original_bytes", result.m_cursor_context.size()},
             {"limit_bytes", cursor_bytes},
             {"result_bytes", trimmed.size()}});
        result.m_cursor_context = trimmed;
    }

    if (cursor_bytes > 0 && result.m_file_context.size() > cursor_bytes)
    {
        const std::string trimmed = trim_utf8_tail(result.m_file_context, cursor_bytes);
        m_logger.log_event(ELogLevel::Info, "context.file_trimmed",
            {{"original_bytes", result.m_file_context.size()},
             {"limit_bytes", cursor_bytes},
             {"result_bytes", trimmed.size()}});
        result.m_file_context = trimmed;
    }

    if (history_bytes > 0 && !result.m_history_context.empty())
    {
        std::size_t total = 0;
        for (const SConversationTurn& turn : result.m_history_context)
        {
            total += turn.m_user_text.size() + turn.m_assistant_text.size();
        }
        if (total > history_bytes)
        {
            // Drop the oldest selected pairs: the newest carry the conversation.
            std::vector<SConversationTurn> kept;
            std::size_t kept_bytes = 0;
            for (auto it = result.m_history_context.rbegin();
                 it != result.m_history_context.rend(); ++it)
            {
                const std::size_t pair_bytes =
                    it->m_user_text.size() + it->m_assistant_text.size();
                if (!kept.empty() && kept_bytes + pair_bytes > history_bytes)
                {
                    break;
                }
                kept_bytes += pair_bytes;
                kept.push_back(*it);
            }
            std::reverse(kept.begin(), kept.end());
            m_logger.log_event(ELogLevel::Info, "context.history_trimmed",
                {{"original_turns", result.m_history_context.size()},
                 {"kept_turns", kept.size()},
                 {"limit_bytes", history_bytes}});
            result.m_history_context = std::move(kept);
        }
    }
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

    // An anonymous namespace, an unnamed struct or an unnamed enum has a USR but
    // no name, so it cannot be a fact subject: the index is keyed by name. This
    // is not a failure, but it must be visible — otherwise the log shows zero
    // sections with an empty entity and looks like a broken index.
    if (result.m_cursor_symbol->m_name.empty()
        && result.m_cursor_symbol->m_qualified_name.empty())
    {
        m_logger.log_event(ELogLevel::Info, "facts.skipped",
            {{"request_id", request.m_request_id},
             {"reason", "entity under cursor has no name"},
             {"kind", symbol_kind_name(result.m_cursor_symbol->m_kind)},
             {"file", std::filesystem::path(
                 result.m_cursor_symbol->m_location.m_file).filename().string()},
             {"line", result.m_cursor_symbol->m_location.m_line}});
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
        // The bounded slice was produced by the relevance assessment; reuse it
        // instead of reading the same file a second time.
        prompt = "FILE CONTENT (bounded slice):\n" + result.m_file_context;
        m_logger.log_event(ELogLevel::Info, "prompt.composed",
            {{"request_id", request.m_request_id},
             {"stage", "file_answer"},
             {"mode", "bounded_slice"},
             {"slice_bytes", result.m_file_context.size()},
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
        // Assessment limits, not the answering role's budget: the route is not
        // chosen yet, so the role is unknown. The slice is re-fitted to the real
        // budget afterwards, in fit_material_to_budget().
        const SFileSliceLimits slice_limits;
        const SFileContextResult assessed =
            m_cursor_analyzer->assess_file(request.m_text, request.m_cursor_file,
                                           slice_limits);
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
        const SCursorSliceLimits cursor_limits;
        const SCursorContextResult assessed = m_cursor_analyzer->assess(
            request.m_text, request.m_cursor_file, request.m_cursor_line,
            request.m_cursor_column, request.m_cursor_compile_arguments,
            cursor_limits);
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
        // Assessment cap, not the answering role's share: the historian already
        // owns its own tail limit. The final budget is enforced afterwards.
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
        // The concrete failure reason must survive into the result: otherwise the
        // caller reports a cursor failure for a broken history or unreadable file.
        result.m_exit_reason = first_failure;
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

    // The user's question must fit before any context is gathered: answering a
    // request that cannot be answered wastes index reads and model calls. The
    // route is still unknown, so the check runs against every possible answering
    // role, not against one hard-coded window.
    if (!check_request_fits(normalised, result))
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        m_logger.log(ELogLevel::Error, result, normalised.m_request_id,
                     normalised.m_session_id, elapsed);
        return result;
    }

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

        // The answering role is known only now. Budget must be planned for it,
        // otherwise a route answered by the local model is sized against the
        // remote window and the context overflows past the budget check.
        const bool budget_known = m_llm != nullptr;
        if (budget_known)
        {
            plan_budget(answer_role_for_route(result.m_route), normalised, result);
        }

        // Without an LLM layer there is no window to exceed; the answer step
        // reports the missing layer itself. Only a real budget refusal is fatal.
        if (budget_known && !result.m_budget.m_ok)
        {
            fail(normalised, result, EExitReason::RequestTooLong,
                 result.m_budget.m_error, ETraceEvent::ExitRequestTooLong);
        }
        else if (budget_known && !fit_material_to_budget(result))
        {
            fail(normalised, result, EExitReason::RequestTooLong,
                 "Collected context does not fit the answering model budget.",
                 ETraceEvent::ExitRequestTooLong);
        }
        else
        {
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
                    // Narrow path: the entity is named by the model and confirmed
                    // by the index. Wide path: the bounded file slice goes to the
                    // answer model.
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
    }

    // Terminal trace event: exactly one, unless the failure site already
    // recorded the matching one.
    ETraceEvent terminal = ETraceEvent::ExitCompleted;
    switch (result.m_exit_reason)
    {
    case EExitReason::EmptyRequest:
        terminal = ETraceEvent::ExitEmptyRequest;
        break;
    case EExitReason::RequestTooLong:
        terminal = ETraceEvent::ExitRequestTooLong;
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
