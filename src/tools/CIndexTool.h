/**
 * @file CIndexTool.h
 * @brief Детерминированный инструмент над индексом проекта.
 *
 * Один экземпляр соответствует одному EToolName и выполняет соответствующий
 * запрос к графу/индексу. Модель не участвует в выборе: инструмент вызывается
 * planner'ом с типизированным запросом.
 */

#ifndef FCPP_AGENT_CINDEXTOOL_H
#define FCPP_AGENT_CINDEXTOOL_H

#include <string>

#include "fcpp-agent/ITool.h"

namespace fcpp
{

/**
 * @brief Инструмент доступа к индексу.
 */
class CIndexTool final : public ITool
{
public:
    /**
     * @brief Конструктор.
     * @param name Имя инструмента.
     */
    explicit CIndexTool(EToolName name);

    /**
     * @brief Деструктор.
     */
    ~CIndexTool() override = default;

    /**
     * @brief Имя инструмента.
     * @return Имя.
     */
    EToolName name() const override;

    /**
     * @brief Выполнить инструмент.
     * @param request Запрос.
     * @param context Контекст доступа к индексу.
     * @return Результат либо типизированная ошибка.
     */
    SToolResult execute(const SToolRequest& request, CToolContext& context) const override;

private:
    /**
     * @brief Разрешить цель запроса в USR.
     * @param request Запрос.
     * @param context Контекст.
     * @param out_usr Выходной USR.
     * @param out_candidates Кандидаты при неоднозначности (для модели).
     * @param out_error Выходная ошибка.
     * @return true при успехе.
     */
    bool resolve_usr(const SToolRequest& request,
                     CToolContext& context,
                     std::string& out_usr,
                     std::vector<SSymbol>& out_candidates,
                     SError& out_error) const;

    /**
     * @brief Применить лимит к результату.
     * @param request Запрос.
     * @param result Результат.
     */
    static void apply_limit(const SToolRequest& request, SToolResult& result);

    EToolName m_name; ///< Имя инструмента.
};

} // namespace fcpp

#endif // FCPP_AGENT_CINDEXTOOL_H
