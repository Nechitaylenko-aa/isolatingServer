/**
 * @file SLlmRoleConfig.cpp
 * @brief Имена ролей LLM и разрешение alias-ссылок (Секция 3 rules.md).
 */

#include "fcpp-agent/SLlmRoleConfig.h"

#include <array>
#include <functional>
#include <string>

namespace fcpp
{
namespace
{
/** @brief Имена ролей, индексированные по ELlmRole. */
static const char* const m_role_names[llm_role_count]
{
    "CODER_SMART",
    "CODER_LOCAL",
    "THINKER_SMART",
    "THINKER_LOCAL"
};

/**
 * @brief Состояние обхода роли при разрешении alias-графа.
 */
enum class EVisit
{
    Unseen,   ///< Роль ещё не посещена.
    Visiting, ///< Роль в текущем стеке обхода (цикл).
    Done      ///< Роль разрешена.
};
} // namespace

const char* llm_role_name(ELlmRole role)
{
    const auto index = static_cast<std::size_t>(role);
    return index < llm_role_count ? m_role_names[index] : "UNKNOWN";
}

bool llm_role_from_name(const std::string& name, ELlmRole& role)
{
    for (std::size_t index = 0; index < llm_role_count; ++index)
    {
        if (name == m_role_names[index])
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
        role.m_resolved_type.clear();
        role.m_resolved_context_window_tokens = 0;
        role.m_resolved_output_reserve_tokens = 0;
        role.m_resolved_role = ELlmRole::Count;
        role.m_configured = false;

        const bool has_parameters = !role.m_api.empty()
            || !role.m_model.empty() || !role.m_key.empty();

        // Alias has priority over api/model/key/type. With an alias present the
        // remaining fields are ignored rather than treated as a conflict.
        if (!role.m_alias.empty())
        {
            configured[index] = true;
        }
        else if (has_parameters)
        {
            // Only the endpoint is mandatory. A model name may be omitted for
            // OpenAI-compatible local servers, which substitute the loaded model.
            if (role.m_api.empty())
            {
                error = std::string("role ") + m_role_names[index]
                    + " requires api when alias is empty";
                return false;
            }
            if (role.m_type == "claude" && role.m_model.empty())
            {
                error = std::string("role ") + m_role_names[index]
                    + " requires model for the claude provider";
                return false;
            }
            configured[index] = true;
        }
    }

    std::function<bool(std::size_t)> resolve = [&](std::size_t index) -> bool
    {
        if (!configured[index])
        {
            return true;
        }
        if (visits[index] == EVisit::Visiting)
        {
            error = std::string("LLM role alias cycle at ") + m_role_names[index];
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
            role.m_resolved_type = role.m_type;
            role.m_resolved_context_window_tokens = role.m_context_window_tokens;
            role.m_resolved_output_reserve_tokens = role.m_output_reserve_tokens;
            role.m_resolved_role = static_cast<ELlmRole>(index);
            role.m_configured = true;
        }
        else
        {
            ELlmRole target_role = ELlmRole::Count;
            if (!llm_role_from_name(role.m_alias, target_role))
            {
                error = std::string("unknown alias target '") + role.m_alias
                    + "' for " + m_role_names[index];
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
            role.m_resolved_api = target.m_resolved_api;
            role.m_resolved_model = target.m_resolved_model;
            role.m_resolved_key = target.m_resolved_key;
            role.m_resolved_type = target.m_resolved_type;
            role.m_resolved_context_window_tokens = target.m_resolved_context_window_tokens;
            role.m_resolved_output_reserve_tokens = target.m_resolved_output_reserve_tokens;
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
