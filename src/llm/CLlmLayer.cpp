#include "llm/CLlmLayer.h"

#include <utility>
#include <vector>

#include "llm/CModelOpenAICompat.h"
#include "fcpp-agent/SLogEvent.h"

namespace fcpp
{
namespace
{
const char* system_prompt_for(EPromptType type)
{
    switch (type)
    {
    case EPromptType::CppAssistant:
        return "You are a helpful, precise C++ software engineering assistant. "
               "Answer the user's question directly in the user's language. "
               "Use only the information supplied in the request. "
               "Do not claim to have inspected files or run commands. "
               "State assumptions when needed and do not invent project-specific facts.";
    case EPromptType::YesNo:
        return "Determine whether the statement or condition in the user's request is true. "
               "Respond with exactly YES or NO, with no explanation.";
    case EPromptType::CursorRelevance:
        return "Determine whether the symbol at the editor cursor is relevant to answering "
               "the user's question. Respond with exactly YES or NO, with no explanation.";
    case EPromptType::HistoryRelevance:
        return "Determine whether the supplied conversation history is needed to answer "
               "the user's current question. Respond with exactly YES or NO, with no explanation.";
    case EPromptType::ScopeClassification:
        return "Classify the requested change scope. Respond with exactly one label: "
               "LOCAL, FILE, PROJECT, or REPOSITORY. Do not add explanation.";
    }
    return "";
}

std::string trim_ascii(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" 	
");
    if (first == std::string::npos)
        return {};
    const std::size_t last = value.find_last_not_of(" 	
");
    return value.substr(first, last - first + 1);
}

std::string uppercase_ascii(std::string value)
{
    for (char& ch : value)
    {
        if (ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 'a' + 'A');
    }
    return value;
}

bool validate_prompt_result(EPromptType type, const std::string& text, std::string& error)
{
    const std::string normalized = uppercase_ascii(trim_ascii(text));
    switch (type)
    {
    case EPromptType::CppAssistant:
        if (!text.empty())
            return true;
        error = "empty assistant answer";
        return false;
    case EPromptType::YesNo:
    case EPromptType::CursorRelevance:
    case EPromptType::HistoryRelevance:
        if (normalized == "YES" || normalized == "NO")
            return true;
        error = "expected exactly YES or NO";
        return false;
    case EPromptType::ScopeClassification:
        if (normalized == "LOCAL" || normalized == "FILE"
            || normalized == "PROJECT" || normalized == "REPOSITORY")
            return true;
        error = "expected LOCAL, FILE, PROJECT, or REPOSITORY";
        return false;
    }
    error = "unknown prompt type";
    return false;
}

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
    m_logger.log(ELogLevel::Error, SLogEvent::operation_failed(
        std::string("llm.") + llm_role_name(role),
        reason + (escalated ? " (escalation failed)" : "")));
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
    ELlmRole role, EPromptType prompt_type, const std::string& request)
{
    const char* system_prompt = system_prompt_for(prompt_type);
    if (*system_prompt == '\\0')
    {
        SLlmExecutionResult result;
        result.m_error = "invalid prompt type";
        result.m_used_role = role;
        log_failure(role, result.m_error, false);
        return result;
    }

    return execute(role, system_prompt, request,
        [prompt_type](const std::string& text, std::string& error)
        {
            return validate_prompt_result(prompt_type, text, error);
        });
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
