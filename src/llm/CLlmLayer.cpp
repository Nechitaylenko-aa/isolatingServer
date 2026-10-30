#include "llm/CLlmLayer.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "llm/CEndpointUtil.h"
#include "llm/CModelClaude.h"
#include "llm/CModelOpenAICompat.h"
#include "llm/CUrlTransport.h"
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
    case EPromptType::CppExpert:
        return "You are a senior C++ engineer with expert knowledge of modern C++17/20/23, "
               "RAII, object lifetime, templates, concurrency, the standard library, "
               "undefined behavior, and performance. Answer in the user's language. "
               "Treat supplied cursor source as primary evidence and distinguish visible "
               "facts from assumptions. Explain code precisely and propose compilable C++ "
               "when asked, accounting for ownership, exceptions, and edge cases. "
               "Do not claim to inspect files or run tools beyond the supplied context.";
    case EPromptType::ConversationAssistant:
        return "You are a helpful, precise conversation-aware assistant. Answer the current "
               "user request in the user's language. Use the supplied prior question-answer "
               "pairs only as relevant context, preserving prior constraints and decisions. "
               "The current request is the immediate task; do not blindly repeat old answers. "
               "If history does not establish a fact, say so rather than inventing it. "
               "Do not claim to inspect files or run tools beyond the supplied context.";
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
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
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
    case EPromptType::CppExpert:
    case EPromptType::ConversationAssistant:
        if (!trim_ascii(text).empty())
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

/** @brief Default completion limit when a role does not provide its own. */
constexpr int k_default_max_tokens = 4096;

/**
 * @brief Бюджет контекста, когда провайдер не сообщил окно.
 *
 * Для удалённых endpoint проба `/props` невозможна, поэтому окно берётся из
 * конфига, а при его отсутствии — это значение. Оно намеренно больше 4096:
 * иначе резерв под ответ съедает весь промпт, и запрос режется до нескольких
 * сотен токенов.
 */
constexpr std::size_t k_fallback_context_window = 8192;

/** @brief Таймаут служебного запроса определения окна (сек). */
constexpr long k_probe_timeout_seconds = 5;

/**
 * @brief Проверить, что endpoint локальный (llama.cpp-подобный сервер).
 * @param base_url Endpoint.
 * @return true для localhost/127.0.0.1/[::1].
 */
bool is_local_endpoint(const std::string& base_url)
{
    return base_url.find("127.0.0.1") != std::string::npos
        || base_url.find("localhost") != std::string::npos
        || base_url.find("[::1]") != std::string::npos;
}

/**
 * @brief Взять не больше max_bytes байт с начала строки по границе UTF-8.
 * @param text Исходная строка.
 * @param max_bytes Лимит байт.
 * @return Префикс без разрезанного символа UTF-8.
 */
std::string utf8_head(const std::string& text, std::size_t max_bytes)
{
    if (text.size() <= max_bytes)
    {
        return text;
    }
    std::size_t end = max_bytes;
    while (end > 0 && end < text.size()
           && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U)
    {
        --end;
    }
    return text.substr(0, end);
}

/**
 * @brief Взять не больше max_bytes байт с конца строки по границе UTF-8.
 * @param text Исходная строка.
 * @param max_bytes Лимит байт.
 * @return Суффикс без разрезанного символа UTF-8.
 */
std::string utf8_tail(const std::string& text, std::size_t max_bytes)
{
    if (text.size() <= max_bytes)
    {
        return text;
    }
    std::size_t start = text.size() - max_bytes;
    while (start < text.size()
           && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
    {
        ++start;
    }
    return text.substr(start);
}

/**
 * @brief Консервативная оценка токенов по байтам.
 *
 * Точный подсчёт делал бы отдельный запрос к провайдеру на каждый промпт.
 * Здесь оценка намеренно завышена, чтобы обрезка срабатывала раньше, чем
 * сервер молча обрежет промпт сам.
 *
 * @param text Текст.
 * @return Оценка токенов.
 */
std::size_t estimate_tokens(const std::string& text)
{
    return (text.size() + 2U) / 3U + 4U;
}

/**
 * @brief Привести промпт к бюджету токенов, сохранив начало и конец.
 *
 * Начало промпта содержит контекст (код/историю), конец — сам вопрос
 * пользователя. Поэтому при переполнении выбрасывается середина, а не хвост:
 * иначе модель теряет либо улику, либо вопрос.
 *
 * @param text Промпт.
 * @param budget_tokens Бюджет промпта в токенах.
 * @param out_truncated Признак фактической обрезки.
 * @param out_dropped_bytes Сколько байт выброшено.
 * @return Промпт, уложенный в бюджет.
 */
std::string fit_to_budget(const std::string& text,
                          std::size_t budget_tokens,
                          bool& out_truncated,
                          std::size_t& out_dropped_bytes)
{
    out_truncated = false;
    out_dropped_bytes = 0;
    if (budget_tokens == 0 || estimate_tokens(text) <= budget_tokens)
    {
        return text;
    }

    const std::size_t allowed_bytes = budget_tokens * 3U;
    if (text.size() <= allowed_bytes)
    {
        return text;
    }

    const std::string marker = "\n[... prompt truncated to fit the model context window ...]\n";
    if (allowed_bytes <= marker.size() + 128U)
    {
        out_truncated = true;
        out_dropped_bytes = text.size();
        return utf8_head(text, allowed_bytes);
    }

    const std::size_t usable = allowed_bytes - marker.size();
    const std::size_t head_bytes = (usable * 3U) / 4U;
    const std::size_t tail_bytes = usable - head_bytes;
    out_truncated = true;
    out_dropped_bytes = text.size() - (head_bytes + tail_bytes);
    return utf8_head(text, head_bytes) + marker + utf8_tail(text, tail_bytes);
}

/**
 * @brief Убрать хвост /v1 из base_url для служебных путей llama.cpp.
 * @param base_url Endpoint до /v1.
 * @return Корень сервера.
 */
std::string server_root(const std::string& base_url)
{
    const std::string suffix = "/v1";
    if (base_url.size() >= suffix.size()
        && base_url.compare(base_url.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        return base_url.substr(0, base_url.size() - suffix.size());
    }
    return base_url;
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

std::size_t CLlmLayer::probe_context_window(size_t role_index) const
{
    const SLlmModelRole& config = m_roles.m_roles[role_index];
    if (config.m_resolved_api.empty() || !is_local_endpoint(config.m_resolved_api))
    {
        return 0;
    }

    const CUrlTransport transport;
    const std::string url =
        server_root(config.m_resolved_api) + "/props";
    const SHttpResult reply = transport.get(url, {}, k_probe_timeout_seconds);
    if (!reply.m_ok)
    {
        return 0;
    }

    try
    {
        const nlohmann::json document = nlohmann::json::parse(reply.m_body);
        if (!document.contains("default_generation_settings")
            || !document["default_generation_settings"].is_object())
        {
            return 0;
        }
        const nlohmann::json& settings = document["default_generation_settings"];
        if (!settings.contains("n_ctx") || !settings["n_ctx"].is_number_integer())
        {
            return 0;
        }
        const std::int64_t value = settings["n_ctx"].get<std::int64_t>();
        return value > 0 ? static_cast<std::size_t>(value) : 0;
    }
    catch (const nlohmann::json::exception&)
    {
        return 0;
    }
}

std::size_t CLlmLayer::context_window_tokens(ELlmRole role) const
{
    if (role == ELlmRole::Count)
    {
        return 0;
    }
    const auto role_index = static_cast<std::size_t>(role);
    const SLlmModelRole& config = m_roles.m_roles[role_index];

    // Explicit configuration always wins.
    if (config.m_resolved_context_window_tokens > 0)
    {
        return config.m_resolved_context_window_tokens;
    }

    if (m_context_windows[role_index] > 0)
    {
        return m_context_windows[role_index];
    }

    const std::size_t probed = probe_context_window(role_index);
    if (probed > 0)
    {
        m_context_windows[role_index] = probed;
        m_logger.log(ELogLevel::Info, SLogEvent("llm.context_window",
            {{"role", llm_role_name(role)}, {"tokens", probed},
             {"source", "provider"}}));
        return probed;
    }

    // Never treat an unknown window as unlimited: fall back to a configured
    // default and record the fact so the omission is visible and fixable.
    const bool local = is_local_endpoint(m_roles.at(role).m_resolved_api);
    m_logger.log(ELogLevel::Warn, SLogEvent("llm.context_window",
        {{"role", llm_role_name(role)}, {"tokens", k_fallback_context_window},
         {"source", "fallback"},
         {"endpoint", local ? "local" : "remote"},
         {"probe", local ? "failed" : "unsupported"},
         {"hint", "set llm_<role>_context_window_tokens in agent.conf"}}));
    return k_fallback_context_window;
}

std::size_t CLlmLayer::output_reserve_tokens(ELlmRole role) const
{
    if (role == ELlmRole::Count)
    {
        return 0;
    }
    const std::size_t window = context_window_tokens(role);
    std::size_t reserve = m_roles.at(role).m_resolved_output_reserve_tokens;
    if (window == 0)
    {
        return reserve;
    }
    // The same clamp as in execute(): the prompt keeps at least half the window.
    const std::size_t minimum_prompt = window / 2U;
    if (reserve >= window - minimum_prompt)
    {
        reserve = window > minimum_prompt ? window - minimum_prompt : 0;
    }
    return reserve;
}

std::unique_ptr<IModelApi> CLlmLayer::make_model(ELlmRole role) const
{
    if (m_factory)
    {
        return m_factory(role);
    }

    if (role == ELlmRole::Count)
    {
        return nullptr;
    }

    const SLlmModelRole& config = m_roles.at(role);
    if (!config.m_configured || config.m_resolved_api.empty())
    {
        return nullptr;
    }
    // A missing model name is valid only for OpenAI-compatible local servers,
    // which fall back to the loaded model. Claude requires an explicit model.
    if (config.m_resolved_model.empty() && config.m_resolved_type == "claude")
    {
        return nullptr;
    }

    SModelEndpoint endpoint;
    endpoint.m_base_url = config.m_resolved_api;
    endpoint.m_model = config.m_resolved_model;
    endpoint.m_api_key = config.m_resolved_key;

    const std::size_t reserve = config.m_resolved_output_reserve_tokens == 0
        ? static_cast<std::size_t>(k_default_max_tokens)
        : config.m_resolved_output_reserve_tokens;
    const int max_tokens = static_cast<int>(reserve);

    if (config.m_resolved_type == "claude")
    {
        return std::make_unique<CModelClaude>(
            std::move(endpoint), 0.0, max_tokens);
    }
    return std::make_unique<CModelOpenAICompat>(
        std::move(endpoint), 0.0, max_tokens);
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

    // Whether the last attempt was damaged by prompt truncation: an empty reply
    // to a truncated prompt says nothing about the model's ability, so escalating
    // to a second role with the same broken prompt only wastes a full timeout.
    bool last_attempt_truncated = false;

    const auto attempt = [&](ELlmRole selected, std::string& text, std::string& error)
    {
        last_attempt_truncated = false;
        std::unique_ptr<IModelApi> model = make_model(selected);
        if (!model || !model->is_available())
        {
            error = std::string("model unavailable: ") + llm_role_name(selected);
            return false;
        }

        // The layer owns the budget: it knows this role's context window and
        // output reserve. Callers supply the prompt; sizing is not their job.
        const std::size_t window = context_window_tokens(selected);
        const std::size_t configured_reserve =
            m_roles.at(selected).m_resolved_output_reserve_tokens;

        // A reserve that leaves nothing for the prompt is a configuration
        // mistake, not a budget: clamp it so the prompt keeps at least half the
        // window. Silently truncating every prompt to a few hundred tokens
        // destroys the request itself.
        const std::size_t minimum_prompt = window > 0 ? window / 2U : 0;
        std::size_t reserve = configured_reserve;
        if (window > 0 && reserve >= window - minimum_prompt)
        {
            reserve = window > minimum_prompt ? window - minimum_prompt : 0;
            m_logger.log(ELogLevel::Warn, SLogEvent("llm.reserve_clamped",
                {{"role", llm_role_name(selected)},
                 {"window_tokens", window},
                 {"configured_reserve", configured_reserve},
                 {"effective_reserve", reserve}}));
        }

        const std::size_t system_tokens = estimate_tokens(system_prompt);
        std::size_t prompt_budget = 0;
        if (window > reserve)
        {
            prompt_budget = window - reserve;
        }
        if (prompt_budget > system_tokens)
        {
            prompt_budget -= system_tokens;
        }
        else
        {
            prompt_budget = 0;
        }

        bool truncated = false;
        std::size_t dropped_bytes = 0;
        const std::string fitted_request =
            fit_to_budget(request, prompt_budget, truncated, dropped_bytes);
        last_attempt_truncated = truncated;
        if (truncated)
        {
            m_logger.log(ELogLevel::Warn, SLogEvent("llm.prompt_truncated",
                {{"role", llm_role_name(selected)},
                 {"window_tokens", window},
                 {"reserve_tokens", reserve},
                 {"budget_tokens", prompt_budget},
                 {"dropped_bytes", dropped_bytes},
                 {"request_bytes", request.size()}}));
        }

        const std::vector<SModelMessage> messages{
            {EModelRole::System, system_prompt, {}, {}},
            {EModelRole::User, fitted_request, {}, {}}
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

    if (last_attempt_truncated)
    {
        // The prompt, not the model, is the reason the answer was unusable.
        // Repeating the same over-long prompt on another role would fail the
        // same way, only after another full timeout.
        result.m_error = primary_error
            + "; not escalating: the prompt did not fit the role context window";
        result.m_used_role = role;
        log_failure(role, result.m_error, false);
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
            SLogEvent::llm_escalated(llm_role_name(role), llm_role_name(fallback),
                                     primary_error));
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
    if (*system_prompt == '\0')
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
