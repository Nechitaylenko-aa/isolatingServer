/**
 * @file CToolRegistry.h
 * @brief Реестр детерминированных инструментов (Этап 5).
 *
 * Регистрирует инструменты по именам и выполняет их по типизированному запросу.
 * Модель не выбирает инструменты: выбор выполняет deterministic planner.
 */

#ifndef FCPP_AGENT_CTOOLREGISTRY_H
#define FCPP_AGENT_CTOOLREGISTRY_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "fcpp-agent/ITool.h"

namespace fcpp
{

class CToolContext;

/**
 * @brief Реестр инструментов.
 */
class CToolRegistry
{
public:
    /**
     * @brief Конструктор: регистрирует все поддерживаемые инструменты.
     */
    CToolRegistry();

    /**
     * @brief Деструктор.
     */
    ~CToolRegistry();

    /**
     * @brief Копирование запрещено.
     */
    CToolRegistry(const CToolRegistry&) = delete;

    /**
     * @brief Присваивание запрещено.
     * @return Ссылка на текущий объект.
     */
    CToolRegistry& operator=(const CToolRegistry&) = delete;

    /**
     * @brief Выполнить инструмент по имени.
     * @param name Имя инструмента.
     * @param request Запрос.
     * @param context Контекст доступа к индексу.
     * @return Результат либо типизированная ошибка.
     */
    SToolResult execute(EToolName name, const SToolRequest& request, CToolContext& context) const;

    /**
     * @brief Список зарегистрированных инструментов.
     * @return Имена инструментов.
     */
    std::vector<std::string> names() const;

    /**
     * @brief Разобрать имя инструмента из строки CLI.
     * @param text Текст.
     * @param out_name Выходное имя.
     * @return true если имя распознано.
     */
    static bool parse_tool_name(const std::string& text, EToolName& out_name);

private:
    std::map<EToolName, std::unique_ptr<ITool>> m_tools; ///< Инструменты по именам.
};

} // namespace fcpp

#endif // FCPP_AGENT_CTOOLREGISTRY_H
