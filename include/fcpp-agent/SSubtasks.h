/**
 * @file SSubtasks.h
 * @brief Модель декомпозиции составного запроса на подвопросы (ADR-16, ADR-17).
 *
 * Фаза A (lite) выдаёт только текстовые атомарные подвопросы; маппинг в Intent
 * выполняет ядро на Фазе B. Промежуточные артефакты — conversation context (§49),
 * а не запись в проект.
 */

#ifndef FCPP_AGENT_SSUBTASKS_H
#define FCPP_AGENT_SSUBTASKS_H

#include <cstddef>
#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Один подвопрос составного запроса (Фаза A).
 */
struct SSubTask
{
    std::string m_question; ///< Атомарный подвопрос на естественном языке.
};

/**
 * @brief План декомпозиции составного запроса.
 */
struct SSubtaskPlan
{
    std::vector<SSubTask> m_subtasks; ///< Упорядоченный список подвопросов.
    bool m_compound = false;          ///< Был ли запрос составным (более одного).

    /**
     * @brief Число подвопросов.
     * @return Количество шагов.
     */
    std::size_t size() const;
};

} // namespace fcpp

#endif // FCPP_AGENT_SSUBTASKS_H
