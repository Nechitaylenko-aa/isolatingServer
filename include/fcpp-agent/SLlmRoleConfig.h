#ifndef FCPP_AGENT_SLLMROLECONFIG_H
#define FCPP_AGENT_SLLMROLECONFIG_H
#include <array>
#include <cstddef>
#include <string>
namespace fcpp {
enum class ELlmRole { CoderSmart, CoderLocal, ThinkerSmart, ThinkerLocal, Count };
constexpr std::size_t llm_role_count = static_cast<std::size_t>(ELlmRole::Count);
struct SLlmModelRole {
    std::string m_api, m_model, m_key, m_alias;
    std::string m_resolved_api, m_resolved_model, m_resolved_key;
    ELlmRole m_resolved_role = ELlmRole::Count;
    bool m_configured = false;
};
struct SLlmRoleSet {
    std::array<SLlmModelRole, llm_role_count> m_roles{};
    const SLlmModelRole& at(ELlmRole role) const { return m_roles[static_cast<std::size_t>(role)]; }
    SLlmModelRole& at(ELlmRole role) { return m_roles[static_cast<std::size_t>(role)]; }
};
const char* llm_role_name(ELlmRole role);
bool llm_role_from_name(const std::string& name, ELlmRole& role);
bool is_local_llm_role(ELlmRole role);
bool resolve_llm_role_aliases(SLlmRoleSet& roles, std::string& error);
}
#endif
