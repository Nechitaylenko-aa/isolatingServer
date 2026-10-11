/**
 * @file CContextDiagnostics.h
 * @brief Shared logging for context-budget selections.
 */
#ifndef FCPP_AGENT_CCONTEXTDIAGNOSTICS_H
#define FCPP_AGENT_CCONTEXTDIAGNOSTICS_H

#include <iostream>
#include <string>
#include <vector>

#include "core/CContextManager.h"

namespace fcpp
{

inline void log_context_selection(const std::string& stage, const SContextBuildResult& result)
{
    const auto print_ids = [](const std::vector<std::string>& ids)
    {
        std::cerr << '[';
        for (std::size_t index = 0; index < ids.size(); ++index)
        {
            if (index != 0) std::cerr << ',';
            std::cerr << ids[index];
        }
        std::cerr << ']';
    };

    std::cerr << "fcpp-agent: CONTEXT_SELECTION stage=" << stage
              << " tokens=" << result.m_estimated_tokens << '/' << result.m_budget_tokens
              << " included=";
    print_ids(result.m_included_group_ids);
    std::cerr << " excluded=";
    print_ids(result.m_excluded_group_ids);
    std::cerr << '\n';
}

} // namespace fcpp

#endif // FCPP_AGENT_CCONTEXTDIAGNOSTICS_H
