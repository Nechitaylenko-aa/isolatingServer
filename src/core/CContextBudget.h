/**
 * @file CContextBudget.h
 * @brief Детерминированный распил контекстного бюджета между частями промпта.
 *
 * Правило одно и находится в этом модуле, чтобы лимиты частей не подбирались
 * «на глаз» в разных местах и не могли суммарно превысить окно модели.
 */

#ifndef FCPP_AGENT_CCONTEXTBUDGET_H
#define FCPP_AGENT_CCONTEXTBUDGET_H

#include <cstddef>
#include <string>

namespace fcpp
{

/**
 * @brief Доли бюджета и лимиты частей промпта, в токенах.
 */
struct SContextBudgetSplit
{
    bool m_ok = false;            ///< Запрос укладывается, бюджет распределён.
    std::size_t m_window = 0;     ///< Окно модели.
    std::size_t m_reserve = 0;    ///< Резерв под ответ.
    std::size_t m_total = 0;      ///< Бюджет промпта всего.
    std::size_t m_request = 0;    ///< Сколько занимает сам текст запроса.
    std::size_t m_remaining = 0;  ///< Остаток после запроса.
    std::size_t m_cursor = 0;     ///< Лимит курсора: 70% остатка.
    std::size_t m_history = 0;    ///< Лимит истории: 30% остатка.
    std::string m_error;          ///< Причина отказа.
};

/**
 * @brief Разделить бюджет промпта между запросом, курсором и историей.
 *
 * Правило:
 * 1. Текст запроса входит целиком. Если он не укладывается в бюджет — отказ
 *    `RequestTooLong`, а не молчаливая обрезка вопроса.
 * 2. Остаток делится: 70% курсору (код под курсором или файл), 30% истории.
 *
 * @param window Окно модели в токенах.
 * @param reserve Резерв под ответ в токенах.
 * @param request_tokens Сколько токенов занимает текст запроса.
 * @return Распределение либо отказ.
 */
SContextBudgetSplit split_context_budget(std::size_t window,
                                         std::size_t reserve,
                                         std::size_t request_tokens);

} // namespace fcpp

#endif // FCPP_AGENT_CCONTEXTBUDGET_H
