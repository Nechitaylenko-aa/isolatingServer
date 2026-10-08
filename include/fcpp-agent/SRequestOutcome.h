/**
 * @file SRequestOutcome.h
 * @brief Исход обработки пользовательского запроса (dataflow, 6a).
 *
 * Закрытый класс исходов: любой путь конвейера обязан завершиться одним из них.
 * Неизвестное/непокрытое по умолчанию скатывается в NotUnderstood (безопасно).
 */

#ifndef FCPP_AGENT_SREQUESTOUTCOME_H
#define FCPP_AGENT_SREQUESTOUTCOME_H

#include <string>
#include <vector>

#include "fcpp-agent/SIntent.h"

namespace fcpp
{

/**
 * @brief Класс исхода (монотонная лестница осторожности).
 */
enum class EOutcomeKind
{
    Ok,            ///< Успех, можно исполнять.
    Degrade,       ///< Действуем безопасным подмножеством.
    Clarify,       ///< Нужен ровно один уточняющий вопрос.
    NotUnderstood, ///< Запрос не понят (безопасный терминал).
    Refused,       ///< Понято, но запрещено политикой.
    Failed,        ///< Понято, но исполнение невозможно.
    Count          ///< Маркер количества.
};

/**
 * @brief Стадия конвейера (для диагностики и отображения).
 */
enum class EStage
{
    Received,
    Interpreted,
    Validated,
    Resolved,
    PolicyChecked,
    Planned,
    Count
};

/**
 * @brief Результат обработки запроса.
 */
struct SRequestOutcome
{
    EOutcomeKind m_kind = EOutcomeKind::NotUnderstood; ///< Класс исхода.
    EStage m_stage = EStage::Received;                 ///< Стадия завершения.
    std::string m_message;                             ///< Человекочитаемое сообщение.
    std::vector<std::string> m_options;                ///< Варианты для Clarify.
    SIntent m_intent;                                  ///< Разобранное намерение.
    SPlan m_plan;                                      ///< Детерминированный план.
};

/**
 * @brief Получить машинное имя класса исхода.
 * @param kind Класс.
 * @return Имя.
 */
const char* outcome_kind_name(EOutcomeKind kind);

/**
 * @brief Получить машинное имя стадии.
 * @param stage Стадия.
 * @return Имя.
 */
const char* stage_name(EStage stage);

} // namespace fcpp

#endif // FCPP_AGENT_SREQUESTOUTCOME_H
