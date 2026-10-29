#include "fcpp-agent/SLlmRoleConfig.h"

#include <array>
#include <functional>
#include <string>

namespace fcpp
{
namespace
{
constexpr std::array<const char*, llm_role_count> k_role_names{
    "CODER_SMART",
    "CODER_LOCAL",
    "THINKER_SMART",
    "THINKER_LOCAL"
};

enum class EVisit
{
    Unseen,
    Visiting,
    Done
};
} // namespace

const char* llm_role_name(ELlmRole role)
{
    const auto index = static_cast<std::size_t>(role);
    return index < k_role_names.size() ? k_role_names[index] : "UNKNOWN";
}

bool llm_role_from_name(const std::string& name, ELlmRole& role)
{
    for (std::size_t index = 0; index < k_role_names.size(); ++index)
    {
        if (name == k_role_names[index])
        {
            role = static_cast<ELlmRole>(index);
            return true;
        }
    }
    return false;
}

bool is_local_llm_role(ELlmRole role)
{
    return role == ELlmRole::CoderLocal || role == ELlmRole::ThinkerLocal;
}

bool resolve_llm_role_aliases(SLlmRoleSet& roles, std::string& error)
{
    error.clear();
    std::array<EVisit, llm_role_count> visits{};
    std::array<bool, llm_role_count> configured{};

    for (std::size_t index = 0; index < llm_role_count; ++index)
    {
        SLlmModelRole& role = roles.m_roles[index];
        role.m_resolved_api.clear();
        role.m_resolved_model.clear();
        role.m_resolved_key.clear();
        role.m_resolved_role = ELlmRole::Count;
        role.m_configured = false;

        const bool has_endpoint = !role.m_api.empty()
            || !role.m_model.empty() || !role.m_key.empty();
        if (!role.m_alias.empty() && has_endpoint)
        {
            error = std::string("role ") + k_role_names[index]
                + " cannot define both endpoint and alias";
            return false;
        }

        if (!role.m_alias.empty())
        {
            configured[index] = true;
        }
        else if (has_endpoint)
        {
            if (role.m_api.empty() || role.m_model.empty())
            {
                error = std::string("role ") + k_role_names[index]
                    + " requires both api and model";
                return false;
            }
            configured[index] = true;
        }
    }

    std::function<bool(std::size_t)> resolve = [&](std::size_t index)
    {
        if (!configured[index])
        {
            return true;
        }
        if (visits[index] == EVisit::Visiting)
        {
            error = std::string("LLM role alias cycle at ") + k_role_names[index];
            return false;
        }
        if (visits[index] == EVisit::Done)
        {
            return true;
        }

        visits[index] = EVisit::Visiting;
        SLlmModelRole& role = roles.m_roles[index];
        if (role.m_alias.empty())
        {
            role.m_resolved_api = role.m_api;
            role.m_resolved_model = role.m_model;
            role.m_resolved_key = role.m_key;
            role.m_resolved_role = static_cast<ELlmRole>(index);
            role.m_configured = true;
        }
        else
        {
            ELlmRole target_role = ELlmRole::Count;
            if (!llm_role_from_name(role.m_alias, target_role))
            {
                error = std::string("unknown alias target '") + role.m_alias
                    + "' for " + k_role_names[index];
                return false;
            }

            const auto target_index = static_cast<std::size_t>(target_role);
            if (!configured[target_index] || !resolve(target_index))
            {
                if (error.empty())
                {
                    error = std::string("alias target is not configured: ") + role.m_alias;
                }
                return false;
            }

            const SLlmModelRole& target = roles.m_roles[target_index];
            if (!target.m_configured)
            {
                error = std::string("alias target is not configured: ") + role.m_alias;
                return false;
            }

            role.m_resolved_api = target.m_resolved_api;
            role.m_resolved_model = target.m_resolved_model;
            role.m_resolved_key = target.m_resolved_key;
            role.m_resolved_role = target.m_resolved_role;
            role.m_configured = true;
        }

        visits[index] = EVisit::Done;
        return true;
    };

    for (std::size_t index = 0; index < llm_role_count; ++index)
    {
        if (configured[index] && !resolve(index))
        {
            return false;
        }
    }
    return true;
}
} // namespace fcpp
