#include "fcpp-agent/SLlmRoleConfig.h"
#include <array>
#include <functional>
namespace fcpp {
namespace {
constexpr std::array<const char*, llm_role_count> kNames{"CODER_SMART","CODER_LOCAL","THINKER_SMART","THINKER_LOCAL"};
enum class Visit { Unseen, Visiting, Done };
}
const char* llm_role_name(ELlmRole role) {
    const auto i=static_cast<std::size_t>(role); return i<kNames.size()?kNames[i]:"UNKNOWN";
}
bool llm_role_from_name(const std::string& name, ELlmRole& role) {
    for(std::size_t i=0;i<kNames.size();++i) if(name==kNames[i]) { role=static_cast<ELlmRole>(i); return true; }
    return false;
}
bool is_local_llm_role(ELlmRole role) { return role==ELlmRole::CoderLocal || role==ELlmRole::ThinkerLocal; }
bool resolve_llm_role_aliases(SLlmRoleSet& roles, std::string& error) {
    error.clear(); std::array<Visit,llm_role_count> visit{}; std::array<bool,llm_role_count> present{};
    for(std::size_t i=0;i<llm_role_count;++i) {
        auto& r=roles.m_roles[i]; r.m_resolved_api.clear(); r.m_resolved_model.clear(); r.m_resolved_key.clear();
        r.m_resolved_role=ELlmRole::Count; r.m_configured=false;
        const bool endpoint=!r.m_api.empty()||!r.m_model.empty()||!r.m_key.empty();
        if(!r.m_alias.empty()&&endpoint) { error=std::string("role ")+kNames[i]+" cannot define both endpoint and alias"; return false; }
        if(!r.m_alias.empty()) present[i]=true;
        else if(endpoint) {
            if(r.m_api.empty()||r.m_model.empty()) { error=std::string("role ")+kNames[i]+" requires both api and model"; return false; }
            present[i]=true;
        }
    }
    std::function<bool(std::size_t)> resolve=[&](std::size_t i) {
        if(!present[i]) return true;
        if(visit[i]==Visit::Visiting) { error=std::string("LLM role alias cycle at ")+kNames[i]; return false; }
        if(visit[i]==Visit::Done) return true;
        visit[i]=Visit::Visiting; auto& r=roles.m_roles[i];
        if(r.m_alias.empty()) {
            r.m_resolved_api=r.m_api; r.m_resolved_model=r.m_model; r.m_resolved_key=r.m_key;
            r.m_resolved_role=static_cast<ELlmRole>(i); r.m_configured=true;
        } else {
            ELlmRole target=ELlmRole::Count;
            if(!llm_role_from_name(r.m_alias,target)) { error=std::string("unknown alias target '")+r.m_alias+"' for "+kNames[i]; return false; }
            const auto j=static_cast<std::size_t>(target);
            if(!present[j]||!resolve(j)) { if(error.empty()) error=std::string("alias target is not configured: ")+r.m_alias; return false; }
            const auto& t=roles.m_roles[j];
            if(!t.m_configured) { error=std::string("alias target is not configured: ")+r.m_alias; return false; }
            r.m_resolved_api=t.m_resolved_api; r.m_resolved_model=t.m_resolved_model; r.m_resolved_key=t.m_resolved_key;
            r.m_resolved_role=t.m_resolved_role; r.m_configured=true;
        }
        visit[i]=Visit::Done; return true;
    };
    for(std::size_t i=0;i<llm_role_count;++i) if(present[i]&&!resolve(i)) return false;
    return true;
}
}
