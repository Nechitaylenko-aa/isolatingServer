/**
 * @file CModelFactory.h
 * @brief Фабрика провайдеров моделей по ролям конфига (6b).
 */

#ifndef FCPP_AGENT_CMODELFACTORY_H
#define FCPP_AGENT_CMODELFACTORY_H

#include <memory>

#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/SConfig.h"

namespace fcpp
{

/**
 * @brief Логическая роль модели (§6).
 */
enum class EModelRoleKind
{
    Main,
    Light,
    Remote,
    Count
};

/**
 * @brief Фабрика моделей.
 */
class CModelFactory
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CModelFactory() = delete;

    /**
     * @brief Создать провайдера для роли по конфигурации.
     *
     * Резолвит `same_as_main` (для light) и создаёт соответствующий провайдер:
     * openai-compat, claude (remote) или OpenAI-совместимый локальный (main/light).
     * @param config Конфигурация.
     * @param role Роль.
     * @return Провайдер либо nullptr при ошибке/недоступности.
     */
    static std::unique_ptr<IModelApi> create(const SConfig& config, EModelRoleKind role);
};

} // namespace fcpp

#endif // FCPP_AGENT_CMODELFACTORY_H
