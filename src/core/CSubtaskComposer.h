/**
 * @file CSubtaskComposer.h
 * @brief Декомпозиция составного запроса на подвопросы — Фаза A (ADR-17).
 *
 * lite-модель выдаёт ТОЛЬКО текстовые атомарные подвопросы (структура).
 * Маппинг в Intent выполняет ядро на Фазе B (CIntentAnalyzer). Промпт запрещает
 * обобщать anchor и обязывает сохранять привязку к курсору/субъекту.
 */

#ifndef FCPP_AGENT_CSUBTASKCOMPOSER_H
#define FCPP_AGENT_CSUBTASKCOMPOSER_H

#include <cstddef>
#include <string>

#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/SSubtasks.h"

namespace fcpp
{

/**
 * @brief Декомпозитор составного запроса (Фаза A).
 */
class CSubtaskComposer
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CSubtaskComposer() = default;

    /**
     * @brief Деструктор.
     */
    ~CSubtaskComposer() = default;

    /**
     * @brief Разложить текст запроса на упорядоченные подвопросы.
     * @param model Провайдер lite-модели.
     * @param user_text Текст запроса.
     * @param cursor_hint Готовый хинт символа под курсором (может быть пустой).
     * @param max_subtasks Жёсткий лимит числа подвопросов.
     * @param out_plan Выходной план декомпозиции.
     * @param out_error Сообщение об ошибке.
     * @return true при успехе разбора.
     */
    bool compose(IModelApi& model,
                 const std::string& user_text,
                 const std::string& cursor_hint,
                 std::size_t max_subtasks,
                 SSubtaskPlan& out_plan,
                 std::string& out_error) const;

    /**
     * @brief Разобрать текстовый ответ модели в список подвопросов (без сети).
     * @param text Текст ответа (по одному подвопросу на строку).
     * @param max_subtasks Жёсткий лимит числа подвопросов.
     * @param out_plan Выходной план.
     * @param out_error Сообщение об ошибке.
     * @return true при успехе.
     */
    static bool parse_subtask_list(const std::string& text,
                                   std::size_t max_subtasks,
                                   SSubtaskPlan& out_plan,
                                   std::string& out_error);
};

} // namespace fcpp

#endif // FCPP_AGENT_CSUBTASKCOMPOSER_H
