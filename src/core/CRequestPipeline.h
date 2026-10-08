/**
 * @file CRequestPipeline.h
 * @brief Конвейер обработки пользовательского запроса (dataflow, 6a).
 *
 * ВСЯ логика стадий и ворот собрана в этом классе, чтобы отладка не требовала
 * перемещения по коду. Принцип: монотонная цепочка — каждый шаг может только
 * ПОНИЗИТЬ исход (Ok -> Degrade -> Clarify/Refused/Failed -> NotUnderstood),
 * но никогда не повысить. Неизвестное/непокрытое -> NotUnderstood (безопасно).
 */

#ifndef FCPP_AGENT_CREQUESTPIPELINE_H
#define FCPP_AGENT_CREQUESTPIPELINE_H

#include <string>

#include "fcpp-agent/SIntent.h"
#include "fcpp-agent/SRequestOutcome.h"

namespace fcpp
{

class CConfigManager;
class CProjectManager;

/**
 * @brief Полный конвейер запроса в одном классе.
 */
class CRequestPipeline
{
public:
    /**
     * @brief Конструктор.
     * @param project_manager Менеджер проектов (может быть nullptr для чистого роутинга).
     * @param config_manager Конфигурация для ворот политики (может быть nullptr).
     */
    CRequestPipeline(CProjectManager* project_manager, const CConfigManager* config_manager);

    /**
     * @brief Деструктор.
     */
    ~CRequestPipeline() = default;

    /**
     * @brief Выполнить полный конвейер для структурированного Intent.
     *
     * Используется для ручной проверки роутинга без LLM (benchmark уровня A).
     * @param intent Намерение.
     * @return Исход обработки.
     */
    SRequestOutcome run(const SIntent& intent) const;

    /**
     * @brief Выполнить конвейер начиная с валидации Intent (интерпретация уже дана).
     * @param intent Намерение.
     * @return Исход обработки.
     */
    SRequestOutcome run_from_intent(const SIntent& intent) const;

private:
    /**
     * @brief Ворота валидации: допустим ли Intent и есть ли маршрут.
     * @param intent Намерение.
     * @return Исход (Ok при успехе; иначе NotUnderstood).
     */
    SRequestOutcome validate(const SIntent& intent) const;

    /**
     * @brief Ворота разрешения субъекта.
     * @param intent Намерение.
     * @return Исход (Ok/Degrade/Clarify/NotUnderstood).
     */
    SRequestOutcome resolve_subject(const SIntent& intent) const;

    /**
     * @brief Ворота политики на основе Effect (§26/§27/§47).
     * @param intent Намерение.
     * @return Исход (Ok/Refused/Degrade).
     */
    SRequestOutcome check_policy(const SIntent& intent) const;

    /**
     * @brief Детерминированный план по Intent (table-driven, §30/§31).
     * @param intent Намерение.
     * @param out_plan Выходной план.
     * @return true если маршрут существует.
     */
    bool plan(const SIntent& intent, SPlan& out_plan) const;

    CProjectManager* m_project_manager;         ///< Менеджер проектов (может быть nullptr).
    const CConfigManager* m_config_manager;     ///< Конфигурация (может быть nullptr).
};

} // namespace fcpp

#endif // FCPP_AGENT_CREQUESTPIPELINE_H
