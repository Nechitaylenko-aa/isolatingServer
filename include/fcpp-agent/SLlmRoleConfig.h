/**
 * @file SLlmRoleConfig.h
 * @brief Логические роли LLM и их конфигурация в agent.conf.
 *
 * Четыре роли: удалённый/локальный кодер и удалённый/локальный мыслитель.
 * Роль либо описывается напрямую (api/model/key/type), либо ссылается на
 * другую роль через alias. Alias имеет приоритет: при непустом alias
 * остальные поля роли игнорируются.
 */

#ifndef FCPP_AGENT_SLLMROLECONFIG_H
#define FCPP_AGENT_SLLMROLECONFIG_H

#include <array>
#include <cstddef>
#include <string>

namespace fcpp
{

/**
 * @brief Логическая роль модели (порядок и имена фиксированы конфигом).
 */
enum class ELlmRole
{
    CoderSmart,   ///< Удалённая платная модель кодера.
    CoderLocal,   ///< Локальная модель кодера для простых операций.
    ThinkerSmart, ///< Удалённая платная модель для типизации и шаблонизации.
    ThinkerLocal, ///< Локальная модель мыслителя.
    Count         ///< Маркер количества ролей.
};

/** @brief Количество логических ролей. */
constexpr std::size_t llm_role_count = static_cast<std::size_t>(ELlmRole::Count);

/**
 * @brief Конфигурация одной логической роли модели.
 */
struct SLlmModelRole
{
    std::string m_api;      ///< Endpoint (api_base), пусто при alias.
    std::string m_model;    ///< Имя модели, пусто при alias.
    std::string m_key;      ///< API-ключ (секрет, не логируется), пусто при alias.
    std::string m_type;     ///< Тип провайдера: openai-compat | claude.
    std::string m_alias;    ///< Имя роли-источника; приоритетнее остальных полей.
    std::size_t m_context_window_tokens = 0; ///< Окно контекста модели; 0 — неизвестно.
    std::size_t m_output_reserve_tokens = 0; ///< Резерв под ответ; 0 — дефолт слоя LLM.

    std::string m_resolved_api;   ///< Итоговый endpoint после разрешения alias.
    std::string m_resolved_model; ///< Итоговое имя модели после разрешения alias.
    std::string m_resolved_key;   ///< Итоговый ключ после разрешения alias.
    std::string m_resolved_type;  ///< Итоговый тип провайдера после разрешения alias.
    std::size_t m_resolved_context_window_tokens = 0; ///< Итоговое окно контекста.
    std::size_t m_resolved_output_reserve_tokens = 0; ///< Итоговый резерв ответа.
    ELlmRole m_resolved_role = ELlmRole::Count; ///< Роль, где реально объявлен endpoint.
    bool m_configured = false;                  ///< Роль разрешена и пригодна к вызову.
};

/**
 * @brief Набор конфигураций всех логических ролей.
 */
struct SLlmRoleSet
{
    std::array<SLlmModelRole, llm_role_count> m_roles{}; ///< Роли по индексу ELlmRole.

    /**
     * @brief Доступ к роли по индексу без проверки границ.
     * @param role Логическая роль.
     * @return Константная ссылка на конфигурацию роли.
     */
    const SLlmModelRole& at(ELlmRole role) const
    {
        return m_roles[static_cast<std::size_t>(role)];
    }

    /**
     * @brief Доступ к роли по индексу без проверки границ.
     * @param role Логическая роль.
     * @return Ссылка на конфигурацию роли.
     */
    SLlmModelRole& at(ELlmRole role)
    {
        return m_roles[static_cast<std::size_t>(role)];
    }
};

/**
 * @brief Получить машинное имя роли.
 * @param role Логическая роль.
 * @return Стабильное имя ("CODER_SMART", ... либо "UNKNOWN").
 */
const char* llm_role_name(ELlmRole role);

/**
 * @brief Разобрать имя роли из строки конфига.
 * @param name Имя роли в верхнем регистре.
 * @param role Выходная роль.
 * @return true если имя распознано.
 */
bool llm_role_from_name(const std::string& name, ELlmRole& role);

/**
 * @brief Проверить, является ли роль локальной (не платной).
 * @param role Логическая роль.
 * @return true для CoderLocal и ThinkerLocal.
 */
bool is_local_llm_role(ELlmRole role);

/**
 * @brief Разрешить alias-ссылки и вычислить итоговые параметры ролей.
 *
 * Роль с непустым alias копирует разрешённые параметры целевой роли, а
 * собственные api/model/key/type игнорируются. Отсутствие api или model у
 * роли без alias — ошибка конфигурации, а не молчаливый пропуск.
 * @param roles Набор ролей для модификации.
 * @param error Выходное описание ошибки.
 * @return true если все роли разрешены корректно.
 */
bool resolve_llm_role_aliases(SLlmRoleSet& roles, std::string& error);

} // namespace fcpp

#endif // FCPP_AGENT_SLLMROLECONFIG_H
