#ifndef FCPP_AGENT_CLLMLAYER_H
#define FCPP_AGENT_CLLMLAYER_H

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/ILogger.h"
#include "fcpp-agent/SLlmRoleConfig.h"

namespace fcpp
{
enum class EPromptType
{
    CppAssistant,          ///< Общий инженерный ответ.
    CppExpert,             ///< Ответ по исходному коду.
    ConversationAssistant, ///< Ответ с опорой на историю диалога.
    YesNo,                 ///< Бинарное решение.
    CursorRelevance,       ///< Применимость курсора.
    HistoryRelevance,      ///< Применимость истории.
    ScopeClassification    ///< Классификация области.
};

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

    /**
     * @brief Размер контекстного окна роли в токенах.
     *
     * Явное значение из конфига имеет приоритет. Если оно не задано (0),
     * окно определяется у провайдера (llama.cpp GET /props → n_ctx) один раз и
     * кэшируется на время жизни слоя. Если провайдер не ответил, возвращается
     * консервативный дефолт, а факт фиксируется в логе. Бесконечным окно не
     * считается никогда.
     *
     * @param role Логическая роль.
     * @return Окно в токенах; 0 только для некорректной роли.
     */
    std::size_t context_window_tokens(ELlmRole role) const;

    // The caller chooses the prompt contract; this layer builds the system prompt.
    SLlmExecutionResult request_text(
        ELlmRole role, EPromptType prompt_type, const std::string& request);

    // Explicit-prompt overload retained for specialized callers.
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
    std::size_t probe_context_window(size_t role_index) const;

    const SLlmRoleSet& m_roles;
    ILogger& m_logger;
    TModelFactory m_factory;

    /// Кэш определённых окон по индексу роли; 0 — ещё не определён.
    mutable std::array<std::size_t, llm_role_count> m_context_windows{};
};
} // namespace fcpp

#endif
