/**
 * @file CContextBudget.cpp
 * @brief Распил контекстного бюджета между частями промпта.
 */

#include "core/CContextBudget.h"

#include <cstddef>
#include <string>

namespace fcpp
{
namespace
{

/** @brief Доля остатка, отдаваемая коду (курсор или содержимое файла). */
constexpr std::size_t k_code_percent = 70;

/** @brief Доля остатка, отдаваемая истории диалога. */
constexpr std::size_t k_history_percent = 30;

} // namespace

SContextBudgetSplit split_context_budget(std::size_t window,
                                         std::size_t reserve,
                                         std::size_t request_tokens)
{
    SContextBudgetSplit split;
    split.m_window = window;
    split.m_reserve = reserve;
    split.m_request = request_tokens;

    if (window == 0)
    {
        split.m_error = "model context window is unknown";
        return split;
    }
    if (reserve >= window)
    {
        split.m_error = "output reserve leaves no room for the prompt";
        return split;
    }

    split.m_total = window - reserve;

    // The user's question is never truncated silently: if it does not fit, the
    // request cannot be answered at all and the caller reports a visible error.
    if (request_tokens >= split.m_total)
    {
        split.m_error = "request text does not fit the model context: request "
            + std::to_string(request_tokens) + " tokens, budget "
            + std::to_string(split.m_total);
        return split;
    }

    split.m_remaining = split.m_total - request_tokens;
    split.m_cursor = split.m_remaining * k_code_percent / 100U;
    split.m_history = split.m_remaining * k_history_percent / 100U;
    split.m_ok = true;
    return split;
}

} // namespace fcpp
