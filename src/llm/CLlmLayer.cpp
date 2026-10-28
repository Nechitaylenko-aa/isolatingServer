#include "llm/CLlmLayer.h"
#include <utility>
#include "llm/CModelOpenAICompat.h"
namespace fcpp {
namespace {
ELlmRole smart_fallback(ELlmRole r) {
    if(r==ELlmRole::CoderLocal) return ELlmRole::CoderSmart;
    if(r==ELlmRole::ThinkerLocal) return ELlmRole::ThinkerSmart;
    return ELlmRole::Count;
}
}
CLlmLayer::CLlmLayer(const SLlmRoleSet& roles,ILogger& logger,TModelFactory factory)
    :m_roles(roles),m_logger(logger),m_factory(std::move(factory)) {}
std::unique_ptr<IModelApi> CLlmLayer::make_model(ELlmRole role) const {
    if(m_factory) return m_factory(role);
    const auto& c=m_roles.at(role);
    if(!c.m_configured||c.m_resolved_api.empty()||c.m_resolved_model.empty()) return nullptr;
    SModelEndpoint endpoint; endpoint.m_base_url=c.m_resolved_api;
    endpoint.m_model=c.m_resolved_model; endpoint.m_api_key=c.m_resolved_key;
    return std::make_unique<CModelOpenAICompat>(std::move(endpoint),0.0,512);
}
void CLlmLayer::log_failure(ELlmRole role,const std::string& request,const std::string& reason,bool escalated) {
    m_logger.log(ELogLevel::Error,std::string("llm.contract_failure role=")+llm_role_name(role)
        +" request="+request+" reason="+reason+" escalated="+(escalated?"true":"false"));
}
SLlmExecutionResult CLlmLayer::execute(ELlmRole role,const std::string& system_prompt,
                                       const std::string& request,const TValidator& validator) {
    SLlmExecutionResult result;
    if(role==ELlmRole::Count) { result.m_error="invalid model role"; log_failure(role,request,result.m_error,false); return result; }
    const auto attempt=[&](ELlmRole selected,std::string& text,std::string& error) {
        auto model=make_model(selected);
        if(!model||!model->is_available()) { error=std::string("model unavailable: ")+llm_role_name(selected); return false; }
        const std::vector<SModelMessage> messages{
            {EModelRole::System,system_prompt,{},{}},{EModelRole::User,request,{},{}}
        };
        const SModelReply reply=model->complete(messages);
        if(!reply.m_ok) { error=reply.m_error.empty()?"provider request failed":reply.m_error; return false; }
        if(!reply.m_tool_calls.empty()) { error="unexpected tool_calls for typed completion"; return false; }
        if(reply.m_finish_reason=="length") { error="response truncated by token limit"; return false; }
        if(reply.m_text.empty()) { error="empty model response"; return false; }
        std::string validation_error;
        if(!validator(reply.m_text,validation_error)) {
            error=validation_error.empty()?"response violates result contract":validation_error; return false;
        }
        text=reply.m_text; return true;
    };
    std::string first_text,first_error;
    if(attempt(role,first_text,first_error)) {
        result.m_ok=true; result.m_text=std::move(first_text); result.m_used_role=role; return result;
    }
    const ELlmRole fallback=smart_fallback(role);
    if(fallback==ELlmRole::Count) {
        result.m_error=first_error; result.m_used_role=role; log_failure(role,request,first_error,false); return result;
    }
    result.m_escalated=true;
    std::string fallback_text,fallback_error;
    if(attempt(fallback,fallback_text,fallback_error)) {
        result.m_ok=true; result.m_text=std::move(fallback_text); result.m_used_role=fallback;
        m_logger.log(ELogLevel::Warn,std::string("llm.escalated from=")+llm_role_name(role)
            +" to="+llm_role_name(fallback)+" reason="+first_error);
        return result;
    }
    result.m_error="primary failed: "+first_error+"; escalation failed: "+fallback_error;
    result.m_used_role=fallback; log_failure(role,request,result.m_error,true); return result;
}
SLlmExecutionResult CLlmLayer::request_text(ELlmRole role,const std::string& system_prompt,const std::string& request) {
    return execute(role,system_prompt,request,[](const std::string& text,std::string& error) {
        if(text.empty()) { error="empty text result"; return false; } return true;
    });
}
}
