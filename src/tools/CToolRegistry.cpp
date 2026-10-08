/**
 * @file CToolRegistry.cpp
 * @brief Реализация реестра инструментов.
 */

#include "tools/CToolRegistry.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "tools/CIndexTool.h"

namespace fcpp
{

CToolRegistry::CToolRegistry()
{
    const EToolName tools[] =
    {
        EToolName::Outline,
        EToolName::Callers,
        EToolName::Callees,
        EToolName::References,
        EToolName::Includes,
        EToolName::Overrides,
        EToolName::Dispatch,
        EToolName::Definition,
        EToolName::Ancestors,
        EToolName::Derived,
        EToolName::Search,
        EToolName::Source
    };

    for (const EToolName tool : tools)
    {
        m_tools[tool] = std::make_unique<CIndexTool>(tool);
    }
}

CToolRegistry::~CToolRegistry() = default;

SToolResult CToolRegistry::execute(EToolName name,
                                   const SToolRequest& request,
                                   CToolContext& context) const
{
    const auto iterator = m_tools.find(name);
    if (iterator == m_tools.end() || iterator->second == nullptr)
    {
        SToolResult result;
        result.m_ok = false;
        result.m_error = EErrorKind::InvalidRequest;
        result.m_message = std::string("tool not registered: ") + tool_name(name);
        return result;
    }

    return iterator->second->execute(request, context);
}

std::vector<std::string> CToolRegistry::names() const
{
    std::vector<std::string> result;
    result.reserve(m_tools.size());
    for (const auto& entry : m_tools)
    {
        result.push_back(tool_name(entry.first));
    }
    return result;
}

bool CToolRegistry::parse_tool_name(const std::string& text, EToolName& out_name)
{
    for (std::size_t index = 0; index < static_cast<std::size_t>(EToolName::Count); ++index)
    {
        const EToolName candidate = static_cast<EToolName>(index);
        if (text == tool_name(candidate))
        {
            out_name = candidate;
            return true;
        }
    }
    return false;
}

} // namespace fcpp
