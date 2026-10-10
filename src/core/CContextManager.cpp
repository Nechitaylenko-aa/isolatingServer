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

SContextBuildResult CContextManager::build(const std::vector<SContextGroup>& groups,
                                           const SContextBudget& budget) const
{
    SContextBuildResult result;
    result.m_budget_tokens = budget.m_max_prompt_tokens;
    if (budget.m_max_prompt_tokens == 0)
    {
        result.m_error = "prompt token budget is zero";
        return result;
    }

    struct Candidate
    {
        const SContextGroup* m_group;
        std::size_t m_tokens;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(groups.size());
    for (const SContextGroup& group : groups)
    {
        if (group.m_id.empty() || group.m_messages.empty())
        {
            result.m_error = "context group must have an id and at least one message";
            return result;
        }
        Candidate candidate{&group, 0};
        for (const SModelMessage& message : group.m_messages)
        {
            candidate.m_tokens += m_token_counter(message);
        }
        candidates.push_back(candidate);
    }

    std::vector<const SContextGroup*> selected;
    std::vector<const SContextGroup*> optional;
    for (const Candidate& candidate : candidates)
    {
        if (candidate.m_group->m_mandatory)
        {
            if (candidate.m_tokens > budget.m_max_prompt_tokens - result.m_estimated_tokens)
            {
                result.m_error = "mandatory context exceeds prompt token budget";
                result.m_messages.clear();
                result.m_included_group_ids.clear();
                result.m_estimated_tokens = 0;
                return result;
            }
            result.m_estimated_tokens += candidate.m_tokens;
            selected.push_back(candidate.m_group);
        }
        else
        {
            optional.push_back(candidate.m_group);
        }
    }

    std::stable_sort(optional.begin(), optional.end(),
        [](const SContextGroup* left, const SContextGroup* right)
        {
            if (left->m_priority != right->m_priority)
            {
                return left->m_priority > right->m_priority;
            }
            if (left->m_relevance != right->m_relevance)
            {
                return left->m_relevance > right->m_relevance;
            }
            if (left->m_recency != right->m_recency)
            {
                return left->m_recency > right->m_recency;
            }
            return left->m_order < right->m_order;
        });

    for (const SContextGroup* group : optional)
    {
        std::size_t group_tokens = 0;
        for (const SModelMessage& message : group->m_messages)
        {
            group_tokens += m_token_counter(message);
        }

        if (group_tokens <= budget.m_max_prompt_tokens - result.m_estimated_tokens)
        {
            result.m_estimated_tokens += group_tokens;
            selected.push_back(group);
        }
        else
        {
            result.m_excluded_group_ids.push_back(group->m_id);
        }
    }

    std::stable_sort(selected.begin(), selected.end(),
        [](const SContextGroup* left, const SContextGroup* right)
        {
            if (left->m_order != right->m_order)
            {
                return left->m_order < right->m_order;
            }
            return left->m_id < right->m_id;
        });

    for (const SContextGroup* group : selected)
    {
        result.m_included_group_ids.push_back(group->m_id);
        result.m_messages.insert(result.m_messages.end(),
                                 group->m_messages.begin(), group->m_messages.end());
    }

    result.m_ok = true;
    return result;
}

} // namespace fcpp
