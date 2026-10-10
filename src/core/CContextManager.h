/**
 * @file CContextManager.h
 * @brief Deterministic selection of complete message groups under a token budget.
 */

#ifndef FCPP_AGENT_CCONTEXTMANAGER_H
#define FCPP_AGENT_CCONTEXTMANAGER_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "fcpp-agent/IModelApi.h"

namespace fcpp
{

/** @brief Atomic candidate group; paired tool calls/results belong in one group. */
struct SContextGroup
{
    std::string m_id;
    std::vector<SModelMessage> m_messages;
    std::size_t m_order = 0;
    int m_priority = 0;
    int m_relevance = 0;
    std::size_t m_recency = 0;
    bool m_mandatory = false;
};

/** @brief Limits for a single model invocation. */
struct SContextBudget
{
    // Optional hard cap for prompt tokens. Zero means no separate prompt cap.
    std::size_t m_max_prompt_tokens = 0;
    // Optional model context window and reserved completion allowance.
    // When configured, the effective prompt budget is min(prompt cap, window - reserve).
    std::size_t m_context_window_tokens = 0;
    std::size_t m_output_reserve_tokens = 0;
};

/** @brief Context build diagnostics and selected model messages. */
struct SContextBuildResult
{
    bool m_ok = false;
    std::string m_error;
    std::vector<SModelMessage> m_messages;
    std::vector<std::string> m_included_group_ids;
    std::vector<std::string> m_excluded_group_ids;
    std::size_t m_estimated_tokens = 0;
    std::size_t m_budget_tokens = 0;
};

/**
 * @brief Selects whole message groups deterministically under a prompt budget.
 *
 * This first-stage component does not summarize content or mutate stored history.
 * Callers supply mandatory policy/task context and scored optional history/evidence.
 * Token counting can be injected by a provider adapter; the default is a rough
 * estimate for fallback use only and is not a substitute for a model tokenizer.
 */
class CContextManager
{
public:
    using TokenCounter = std::function<std::size_t(const SModelMessage&)>;

    explicit CContextManager(TokenCounter token_counter = {});

    SContextBuildResult build(const std::vector<SContextGroup>& groups,
                              const SContextBudget& budget) const;

    static std::size_t estimate_tokens(const SModelMessage& message);

private:
    TokenCounter m_token_counter;
};

} // namespace fcpp

#endif // FCPP_AGENT_CCONTEXTMANAGER_H
