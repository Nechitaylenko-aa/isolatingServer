/**
 * @file CModelFactory.cpp
 * @brief Реализация фабрики моделей (6b).
 */

#include "llm/CModelFactory.h"

#include <memory>
#include <string>

#include "llm/CModelClaude.h"
#include "llm/CModelOpenAICompat.h"

namespace fcpp
{

namespace
{

/** @brief Температура по умолчанию. */
constexpr double k_default_temperature = 0.2;

/** @brief Лимит токенов по умолчанию. */
constexpr int k_default_max_tokens = 4096;

/**
 * @brief Преобразовать роль конфига в endpoint.
 * @param model Параметры модели.
 * @return Endpoint.
 */
SModelEndpoint to_endpoint(const SModelConfig& model)
{
    SModelEndpoint endpoint;
    endpoint.m_base_url = model.m_url;
    endpoint.m_model = model.m_name;
    endpoint.m_api_key = model.m_key;
    return endpoint;
}

} // namespace

std::unique_ptr<IModelApi> CModelFactory::create(const SConfig& config, EModelRoleKind role)
{
    if (role == EModelRoleKind::Remote)
    {
        if (config.m_remote_type == EModelRemoteType::Claude)
        {
            return std::make_unique<CModelClaude>(to_endpoint(config.m_model_remote),
                                                  k_default_temperature, k_default_max_tokens);
        }
        return std::make_unique<CModelOpenAICompat>(to_endpoint(config.m_model_remote),
                                                    k_default_temperature, k_default_max_tokens);
    }

    if (role == EModelRoleKind::Light)
    {
        // same_as_main: light использует параметры main, но пустые значения наследуются.
        SModelConfig light = config.m_model_light;
        if (light.m_url == "same_as_main" || light.m_url.empty())
        {
            light.m_url = config.m_model_main.m_url;
        }
        if (light.m_name.empty())
        {
            light.m_name = config.m_model_main.m_name;
        }
        return std::make_unique<CModelOpenAICompat>(to_endpoint(light),
                                                    k_default_temperature, k_default_max_tokens);
    }

    return std::make_unique<CModelOpenAICompat>(to_endpoint(config.m_model_main),
                                                k_default_temperature, k_default_max_tokens);
}

} // namespace fcpp
