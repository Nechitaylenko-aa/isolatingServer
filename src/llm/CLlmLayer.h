#ifndef FCPP_AGENT_CLLMLAYER_H
#define FCPP_AGENT_CLLMLAYER_H

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/ILogger.h"
#include "fcpp-agent/SLlmRoleConfig.h"

namespace fcpp
{
struct SLlmExecutionResult
{
    bool m_ok = false;
    std::string m_text;
    std::string m_error;
    ELlmRole m_used_role = ELlmRole::Count;
    bool m_escalated = false;
};

template<typename T>
struct SLlmResult
{
    bool m_ok = false;
    T m_value{};
    std::string m_error;
    ELlmRole m_used_role = ELlmRole::Count;
    bool m_escalated = false;
};

class CLlmLayer
{
public:
    using TModelFactory = std::function<std::unique_ptr<IModelApi>(ELlmRole)>;
    using TValidator = std::function<bool(const std::string&, std::string&)>;

    CLlmLayer(const SLlmRoleSet& roles, ILogger& logger, TModelFactory factory = {});

    SLlmExecutionResult request_text(
        ELlmRole role, const std::string& system_prompt, const std::string& request);

    template<typename T>
    SLlmResult<T> request(
        ELlmRole role,
        const std::string& system_prompt,
        const std::string& request_text,
        const std::function<bool(const std::string&, T&, std::string&)>& decoder)
    {
        SLlmResult<T> result;
        T candidate{};

        const TValidator validator = [&](const std::string& text, std::string& error)
        {
            T decoded{};
            if (!decoder(text, decoded, error))
            {
                return false;
            }
            candidate = std::move(decoded);
            return true;
        };

        const SLlmExecutionResult execution =
            execute(role, system_prompt, request_text, validator);
        result.m_ok = execution.m_ok;
        result.m_error = execution.m_error;
        result.m_used_role = execution.m_used_role;
        result.m_escalated = execution.m_escalated;
        if (result.m_ok)
        {
            result.m_value = std::move(candidate);
        }
        return result;
    }

private:
    SLlmExecutionResult execute(
        ELlmRole role,
        const std::string& system_prompt,
        const std::string& request,
        const TValidator& validator);

    std::unique_ptr<IModelApi> make_model(ELlmRole role) const;
    void log_failure(ELlmRole role, const std::string& reason, bool escalated);

    const SLlmRoleSet& m_roles;
    ILogger& m_logger;
    TModelFactory m_factory;
};
} // namespace fcpp

#endif
