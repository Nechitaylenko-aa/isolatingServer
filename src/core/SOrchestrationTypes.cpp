/**
 * @file SOrchestrationTypes.cpp
 * @brief Текстовые имена типов дерева обработки запроса (Секция 3 rules.md).
 */

#include "fcpp-agent/SOrchestrationTypes.h"

#include <cstddef>

namespace fcpp
{
namespace
{

/** @brief Имена статусов, индексированные по ERequestStatus. */
static const char* const m_request_status_names[static_cast<std::size_t>(ERequestStatus::Count)]
{
    "REJECTED",
    "FAILED",
    "COMPLETED"
};

/** @brief Имена причин выхода, индексированные по EExitReason. */
static const char* const m_exit_reason_names[static_cast<std::size_t>(EExitReason::Count)]
{
    "NONE",
    "EMPTY_REQUEST",
    "REQUEST_TOO_LONG",
    "CURSOR_RELEVANCE_FAILED",
    "HISTORY_RELEVANCE_FAILED",
    "FILE_RELEVANCE_FAILED",
    "FILE_NOT_RELEVANT",
    "LLM_GENERATION_FAILED",
    "INDEX_UNAVAILABLE"
};

/** @brief Имена маршрутов, индексированные по EOrchestrationRoute. */
static const char* const m_route_names[static_cast<std::size_t>(EOrchestrationRoute::Count)]
{
    "UNSELECTED",
    "NO_APPLICABLE_CONTEXT",
    "CURSOR_CONTEXT_REQUIRED",
    "HISTORY_CONTEXT_REQUIRED",
    "CURSOR_AND_HISTORY_CONTEXT_REQUIRED",
    "FILE_CONTEXT_REQUIRED",
    "FILE_AND_HISTORY_CONTEXT_REQUIRED"
};

/** @brief Имена событий дерева, индексированные по ETraceEvent. */
static const char* const m_trace_event_names[static_cast<std::size_t>(ETraceEvent::Count)]
{
    "VALIDATION:OK",
    "VALIDATION:FAILED",
    "APPLICABILITY:FAILED",
    "APPLICABILITY:ASSESSED",
    "ROUTE:SELECTED",
    "ANSWER:GENERATED",
    "ANSWER:AUDITED",
    "EXIT:LLM_GENERATION_FAILED",
    "EXIT:COMPLETED",
    "EXIT:EMPTY_REQUEST",
    "EXIT:REQUEST_TOO_LONG",
    "EXIT:CURSOR_RELEVANCE_FAILED",
    "EXIT:HISTORY_RELEVANCE_FAILED",
    "EXIT:FILE_RELEVANCE_FAILED",
    "EXIT:FILE_NOT_RELEVANT",
    "EXIT:INDEX_UNAVAILABLE"
};

} // namespace

const char* request_status_name(ERequestStatus value)
{
    const auto index = static_cast<std::size_t>(value);
    return index < static_cast<std::size_t>(ERequestStatus::Count)
        ? m_request_status_names[index] : "UNKNOWN";
}

const char* exit_reason_name(EExitReason value)
{
    const auto index = static_cast<std::size_t>(value);
    return index < static_cast<std::size_t>(EExitReason::Count)
        ? m_exit_reason_names[index] : "UNKNOWN";
}

const char* orchestration_route_name(EOrchestrationRoute value)
{
    const auto index = static_cast<std::size_t>(value);
    return index < static_cast<std::size_t>(EOrchestrationRoute::Count)
        ? m_route_names[index] : "UNKNOWN";
}

const char* trace_event_name(ETraceEvent value)
{
    const auto index = static_cast<std::size_t>(value);
    return index < static_cast<std::size_t>(ETraceEvent::Count)
        ? m_trace_event_names[index] : "UNKNOWN";
}

} // namespace fcpp
