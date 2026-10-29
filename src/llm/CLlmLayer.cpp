#include "llm/CLlmLayer.h"

#include <utility>
#include <vector>

#include "llm/CModelOpenAICompat.h"

namespace fcpp
{
namespace
{
ELlmRole smart_fallback(ELlmRole role)
{
    switch (role)
    {
        case ELlmRole::CoderLocal:
            return ELlmRole::CoderSmart;
        case ELlmRole::ThinkerLocal:
            return ELlmRole::ThinkerSmart;
        default:
            return ELlmRole::Count;
    }
}
} // namespace

CLlmLayer::CLlmLayer(const SLlmRoleSet& roles, ILogger& logger, TModelFactory factory)
    : m_roles(roles)
    , m_logger(logger)
    , m_factory(std::move(factory))
{
}

std::unique_ptr<IModelApi> CLlmLayer::make_model(ELlmRole role) const
{
    if (m_factory)
    {
        return m_factory(role);
    }

    const SLlmModelRole& config = m_roles.at(role);
    if (!config.m_configured || config.m_resolved_api.empty()
        || config.m_resolved_model.empty())
    {
        return nullptr;
    }

    SModelEndpoint endpoint;
    endpoint.m_base_url = config.m_resolved_api;
    endpoint.m_model = config.m_resolved_model;
    endpoint.m_api_key = config.m_resolved_key;
    return std::make_unique<CModelOpenAICompat>(std::move(endpoint), 0.0, 512);
}

void CLlmLayer::log_failure(ELlmRole role, const std::string& reason, bool escalated)
{
    m_logger.log(ELogLevel::Error,
        std::string("llm.failed role=") + llm_role_name(role)
        + " reason=" + reason
        + " escalated=" + (escalated ? "true" : "false"));
}

SLlmExecutionResult CLlmLayer::execute(
    ELlmRole role,
    const std::string& system_prompt,
    const std::string& request,
    const TValidator& validator)
{
    SLlmExecutionResult result;
    if (role == ELlmRole::Count)
    {
        result.m_error = "invalid model role";
        log_failure(role, result.m_error, false);
        return result;
    }

    const auto attempt = [&](ELlmRole selected, std::string& text, std::string& error)
    {
        std::unique_ptr<IModelApi> model = make_model(selected);
        if (!model || !model->is_available())
        {
            error = std::string("model unavailable: ") + llm_role_name(selected);
            return false;
        }

        const std::vector<SModelMessage> messages{
            {EModelRole::System, system_prompt, {}, {}},
            {EModelRole::User, request, {}, {}}
        };
        const SModelReply reply = model->complete(messages);
        if (!reply.m_ok)
        {
            error = reply.m_error.empty() ? "provider request failed" : reply.m_error;
            return false;
        }
        if (!reply.m_tool_calls.empty())
        {
            error = "unexpected tool calls for typed completion";
            return false;
        }
        if (reply.m_finish_reason == "length")
        {
            error = "response truncated by token limit";
            return false;
        }
        if (reply.m_text.empty())
        {
            error = "empty model response";
            return false;
        }

        std::string validation_error;
        if (!validator(reply.m_text, validation_error))
        {
            error = validation_error.empty()
                ? "response violates result contract" : validation_error;
            return false;
        }

        text = reply.m_text;
        return true;
    };

    std::string primary_text;
    std::string primary_error;
    if (attempt(role, primary_text, primary_error))
    {
        result.m_ok = true;
        result.m_text = std::move(primary_text);
        result.m_used_role = role;
        return result;
    }

    const ELlmRole fallback = smart_fallback(role);
    if (fallback == ELlmRole::Count)
    {
        result.m_error = primary_error;
        result.m_used_role = role;
        log_failure(role, primary_error, false);
        return result;
    }

    result.m_escalated = true;
    std::string fallback_text;
    std::string fallback_error;
    if (attempt(fallback, fallback_text, fallback_error))
    {
        result.m_ok = true;
        result.m_text = std::move(fallback_text);
        result.m_used_role = fallback;
        m_logger.log(ELogLevel::Warn,
            std::string("llm.escalated from=") + llm_role_name(role)
            + " to=" + llm_role_name(fallback)
            + " reason=" + primary_error);
        return result;
    }

    result.m_error = "primary failed: " + primary_error
        + "; escalation failed: " + fallback_error;
    result.m_used_role = fallback;
    log_failure(role, result.m_error, true);
    return result;
}

SLlmExecutionResult CLlmLayer::request_text(
    ELlmRole role, const std::string& system_prompt, const std::string& request)
{
    return execute(role, system_prompt, request,
        [](const std::string& text, std::string& error)
        {
            if (text.empty())
            {
                error = "empty text result";
                return false;
            }
            return true;
        });
}
} // namespace fcpp
