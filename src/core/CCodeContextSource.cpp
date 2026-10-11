/**
 * @file CCodeContextSource.cpp
 * @brief Bounded source gate for code-index evidence.
 */
#include "core/CCodeContextSource.h"

#include <string>
#include <utility>

namespace fcpp
{

CCodeContextSource::CCodeContextSource(const SEvidence& evidence)
    : m_evidence(evidence)
{
}

SContextSourceResult CCodeContextSource::collect_context(
    const SContextSourceRequest& request) const
{
    SContextSourceResult result;
    if (request.m_max_tokens == 0 && !m_evidence.m_sections.empty())
    {
        result.m_complete = false;
        result.m_note = "code evidence allowance is zero";
        return result;
    }

    std::vector<SContextGroup> candidates;
    std::size_t order = 0;
    for (const SEvidenceSection& section : m_evidence.m_sections)
    {
        if (section.m_text.empty())
        {
            continue;
        }
        SModelMessage message;
        message.m_role = EModelRole::User;
        message.m_content = "[" + section.m_title + "]\n" + section.m_text;
        candidates.push_back(SContextGroup{
            "code-evidence-" + std::to_string(++order),
            {std::move(message)}, order, 110, 90, order, false});
    }
    if (candidates.empty())
    {
        return result;
    }

    const CContextManager manager;
    const SContextBuildResult bounded = manager.build(
        candidates, SContextBudget{request.m_max_tokens});
    if (!bounded.m_ok)
    {
        result.m_complete = false;
        result.m_note = "code evidence could not be budgeted: " + bounded.m_error;
        return result;
    }

    for (const std::string& id : bounded.m_included_group_ids)
    {
        for (const SContextGroup& candidate : candidates)
        {
            if (candidate.m_id == id)
            {
                result.m_groups.push_back(candidate);
                break;
            }
        }
    }
    result.m_estimated_tokens = bounded.m_estimated_tokens;
    result.m_complete = bounded.m_excluded_group_ids.empty();
    if (!result.m_complete)
    {
        result.m_note = "some code evidence sections did not fit the source allowance";
    }
    return result;
}

} // namespace fcpp
