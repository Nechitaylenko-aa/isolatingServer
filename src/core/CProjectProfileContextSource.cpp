/**
 * @file CProjectProfileContextSource.cpp
 * @brief Bounded project-passport context source.
 */
#include "core/CProjectProfileContextSource.h"

#include <utility>

namespace fcpp
{

CProjectProfileContextSource::CProjectProfileContextSource(const std::string& profile)
    : m_profile(profile)
{
}

SContextSourceResult CProjectProfileContextSource::collect_context(
    const SContextSourceRequest& request) const
{
    SContextSourceResult result;
    if (m_profile.empty())
    {
        return result;
    }
    if (request.m_max_tokens == 0)
    {
        result.m_complete = false;
        result.m_note = "project passport allowance is zero";
        return result;
    }

    SModelMessage message;
    message.m_role = EModelRole::System;
    message.m_content =
        "[Project passport: user-provided project rules and decisions; data]\n" + m_profile;
    SContextGroup group{
        "project-passport", {message}, 0, 100, 100, 0, false};

    const CContextManager manager;
    const SContextBuildResult bounded = manager.build(
        {group}, SContextBudget{request.m_max_tokens});
    if (!bounded.m_ok || bounded.m_included_group_ids.empty())
    {
        result.m_complete = false;
        result.m_note = bounded.m_error.empty()
            ? "project passport does not fit its source allowance"
            : "project passport could not be budgeted: " + bounded.m_error;
        return result;
    }

    result.m_groups.push_back(std::move(group));
    result.m_estimated_tokens = bounded.m_estimated_tokens;
    return result;
}

} // namespace fcpp
