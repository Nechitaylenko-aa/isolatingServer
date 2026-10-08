/**
 * @file CAgentApi.h
 * @brief Реализация Agent API (§89, §93).
 *
 * Единая точка входа для CLI/IDE/GUI. Не раскрывает внутренние сущности
 * (Intent/Area/Relation/Tool) как обязательные (§89.39) — только типизированные
 * запрос/ответ.
 */

#ifndef FCPP_AGENT_CAGENTAPI_H
#define FCPP_AGENT_CAGENTAPI_H

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/IAgentApi.h"

namespace fcpp
{

/**
 * @brief Реализация Agent API поверх детерминированного ядра.
 */
class CAgentApi final : public IAgentApi
{
public:
    /**
     * @brief Конструктор.
     * @param config_manager Менеджер конфигурации (должен пережить API).
     */
    explicit CAgentApi(const CConfigManager& config_manager);

    /**
     * @brief Деструктор.
     */
    ~CAgentApi() override = default;

    /**
     * @brief Выполнить запрос Agent API.
     * @param request Запрос.
     * @return Ответ.
     */
    SApiResponse execute(const SApiRequest& request) override;

private:
    /**
     * @brief Обработать запрос к инструменту (Tool).
     * @param request Запрос.
     * @return Ответ.
     */
    SApiResponse handle_tool(const SApiRequest& request);

    /**
     * @brief Обработать применение патча.
     * @param request Запрос.
     * @return Ответ.
     */
    SApiResponse handle_patch(const SApiRequest& request);

    const CConfigManager& m_config_manager; ///< Конфигурация.
    CProjectManager m_project_manager;      ///< Менеджер проектов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CAGENTAPI_H
