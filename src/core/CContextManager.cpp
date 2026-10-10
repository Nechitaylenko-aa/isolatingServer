#include "core/CContextManager.h"

#include <algorithm>
#include <utility>

namespace fcpp
{

CContextManager::CContextManager(TokenCounter token_counter)
    : m_token_counter(token_counter ? std::move(token_counter) : TokenCounter(estimate_tokens))
{
}

std::size_t CContextManager::estimate_tokens(const SModelMessage& message)
{
    std::size_t characters = message.m_content.size() + message.m_tool_call_id.size();
    for (const SModelToolCall& call : message.m_tool_calls)
    {
        characters += call.m_id.size() + call.m_name.size() + call.m_arguments.size();
    }
    return (characters + 2U) / 3U + 4U;
}

namespace
{

std::string utf8_prefix(const std::string& text, std::size_t max_bytes)
{
    if (text.size() <= max_bytes)
    {
        return text;
    }
    std::size_t end = max_bytes;
    while (end > 0 && end < text.size()
           && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U)
    {
        --end;
    }
    return text.substr(0, end);
}

} // namespace

SConversationCompaction compact_conversation_history(
    const std::vector<SConversationTurn>& history,
    std::size_t keep_recent_turns,
    std::size_t max_summary_bytes)
{
    SConversationCompaction result;
    const std::size_t recent_count = std::min(keep_recent_turns, history.size());
    const std::size_t older_count = history.size() - recent_count;
    result.m_compacted_turn_count = older_count;
    result.m_recent_turns.assign(
        history.begin() + static_cast<std::vector<SConversationTurn>::difference_type>(older_count),
        history.end());

    for (std::size_t index = 0; index < older_count && max_summary_bytes > result.m_summary_text.size(); ++index)
    {
        const SConversationTurn& turn = history[index];
        std::string entry = "Turn " + std::to_string(index + 1) + " user: " + turn.m_user_text;
        if (!turn.m_assistant_text.empty())
        {
            entry += "\n  assistant: " + turn.m_assistant_text;
        }
        entry += "\n";

        const std::size_t remaining = max_summary_bytes - result.m_summary_text.size();
        if (entry.size() > remaining)
        {
            if (remaining > 0)
            {
                result.m_summary_text += utf8_prefix(entry, remaining);
            }
            break;
        }
        result.m_summary_text += entry;
    }
    return result;
}

SContextBuildResult CContextManager::build(const std::vector<SContextGroup>& groups,
                                           const SContextBudget& budget) const
{
    SContextBuildResult result;

    std::size_t effective_budget = budget.m_max_prompt_tokens;
    if (budget.m_context_window_tokens > 0)
    {
        if (budget.m_output_reserve_tokens >= budget.m_context_window_tokens)
        {
            result.m_error = "output reserve must be smaller than model context window";
            return result;
        }
        const std::size_t prompt_capacity =
            budget.m_context_window_tokens - budget.m_output_reserve_tokens;
        effective_budget = effective_budget == 0
            ? prompt_capacity : std::min(effective_budget, prompt_capacity);
    }
    if (effective_budget == 0)
    {
        result.m_error = "prompt token budget is zero";
        return result;
    }
    result.m_budget_tokens = effective_budget;

    struct Candidate
    {
        const SContextGroup* m_group;
        std::size_t m_tokens;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(groups.size());
    std::vector<std::string> seen_ids;
    seen_ids.reserve(groups.size());
    for (const SContextGroup& group : groups)
    {
        if (group.m_id.empty() || group.m_messages.empty())
        {
            result.m_error = "context group must have an id and at least one message";
            return result;
        }
        if (std::find(seen_ids.begin(), seen_ids.end(), group.m_id) != seen_ids.end())
        {
            result.m_error = "context group ids must be unique";
            return result;
        }
        seen_ids.push_back(group.m_id);

        Candidate candidate{&group, 0};
        for (const SModelMessage& message : group.m_messages)
        {
            candidate.m_tokens += m_token_counter(message);
        }
        candidates.push_back(candidate);
    }

    std::vector<const Candidate*> selected;
    std::vector<const Candidate*> optional;
    for (const Candidate& candidate : candidates)
    {
        if (candidate.m_group->m_mandatory)
        {
            if (candidate.m_tokens > result.m_budget_tokens - result.m_estimated_tokens)
            {
                result.m_error = "mandatory context exceeds prompt token budget";
                result.m_messages.clear();
                result.m_included_group_ids.clear();
                result.m_estimated_tokens = 0;
                return result;
            }
            result.m_estimated_tokens += candidate.m_tokens;
            selected.push_back(&candidate);
        }
        else
        {
            optional.push_back(&candidate);
        }
    }

    std::stable_sort(optional.begin(), optional.end(),
        [](const Candidate* left, const Candidate* right)
        {
            if (left->m_group->m_priority != right->m_group->m_priority)
            {
                return left->m_group->m_priority > right->m_group->m_priority;
            }
            if (left->m_group->m_relevance != right->m_group->m_relevance)
            {
                return left->m_group->m_relevance > right->m_group->m_relevance;
            }
            if (left->m_group->m_recency != right->m_group->m_recency)
            {
                return left->m_group->m_recency > right->m_group->m_recency;
            }
            return left->m_group->m_order < right->m_group->m_order;
        });

    for (const Candidate* candidate : optional)
    {
        const SContextGroup* group = candidate->m_group;
        if (candidate->m_tokens <= result.m_budget_tokens - result.m_estimated_tokens)
        {
            result.m_estimated_tokens += candidate->m_tokens;
            selected.push_back(candidate);
        }
        else
        {
            result.m_excluded_group_ids.push_back(group->m_id);
        }
    }

    std::stable_sort(selected.begin(), selected.end(),
        [](const Candidate* left, const Candidate* right)
        {
            if (left->m_group->m_order != right->m_group->m_order)
            {
                return left->m_group->m_order < right->m_group->m_order;
            }
            return left->m_group->m_id < right->m_group->m_id;
        });

    for (const Candidate* candidate : selected)
    {
        const SContextGroup* group = candidate->m_group;
        result.m_included_group_ids.push_back(group->m_id);
        result.m_messages.insert(result.m_messages.end(),
                                 group->m_messages.begin(), group->m_messages.end());
    }
    result.m_ok = true;
    return result;
}

} // namespace fcpp
