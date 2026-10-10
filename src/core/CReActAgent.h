/**
 * @file CReActAgent.h
 * @brief LLM-driven loop (ReAct): модель управляет вызовом инструментов (ADR-23).
 *
 * Модель получает задачу и схемы инструментов; она решает, какой инструмент вызвать,
 * видит результат и продолжает, пока не ответит. Исполняют инструменты —
 * детерминированный core (CToolRegistry). Guardrails: лимит шагов, валидация
 * аргументов, анти-цикл.
 */

#ifndef FCPP_AGENT_CREACTAGENT_H
#define FCPP_AGENT_CREACTAGENT_H

#include <cstddef>
#include <string>
#include <vector>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "core/CConversationStore.h"
#include "fcpp-agent/IModelApi.h"

namespace fcpp
{

/**
 * @brief Агент цикла ReAct.
 */
class CReActAgent
{
public:
    /**
     * @brief Конструктор.
     * @param config_manager Конфигурация.
     * @param project_manager Менеджер проектов.
     */
    CReActAgent(const CConfigManager& config_manager, CProjectManager& project_manager);

    /**
     * @brief Деструктор.
     */
    ~CReActAgent() = default;

    /**
     * @brief Выполнить запрос в режиме ReAct.
     * @param question Текст запроса.
     * @param cursor_hint Подсказка контекста (символ под курсором / якорь), может быть пустой.
     * @param out_answer Выходной ответ модели.
     * @return true при успехе (получен финальный ответ).
     */
    bool run(const std::string& question,
             const std::string& cursor_hint,
             const std::string& context_json,
             const std::vector<SConversationTurn>& history,
             std::string& out_answer) const;

private:
    const CConfigManager& m_config_manager; ///< Конфигурация.
    CProjectManager& m_project_manager;     ///< Менеджер проектов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CREACTAGENT_H
