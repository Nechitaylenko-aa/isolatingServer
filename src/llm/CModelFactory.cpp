/**
 * @file CModelFactory.cpp
 * @brief Реализация фабрики моделей (6b).
 */

#include "llm/CModelFactory.h"

#include <algorithm>
#include <limits>
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

/** Use the configured completion reserve as the provider's output-token limit. */
int output_token_limit(const SModelConfig& model)
{
    if (model.m_output_reserve_tokens == 0)
    {
        return k_default_max_tokens;
    }
    const std::size_t limit = std::min(
        model.m_output_reserve_tokens,
        static_cast<std::size_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(limit);
}

} // namespace

std::unique_ptr<IModelApi> CModelFactory::create(const SConfig& config, EModelRoleKind role)
{
    if (role == EModelRoleKind::Remote)
    {
        if (config.m_remote_type == EModelRemoteType::Claude)
        {
            return std::make_unique<CModelClaude>(to_endpoint(config.m_model_remote),
                                                  k_default_temperature, output_token_limit(config.m_model_remote));
        }
        return std::make_unique<CModelOpenAICompat>(to_endpoint(config.m_model_remote),
                                                    k_default_temperature, output_token_limit(config.m_model_remote));
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
        if (light.m_context_window_tokens == 0)
        {
            light.m_context_window_tokens = config.m_model_main.m_context_window_tokens;
        }
        if (light.m_output_reserve_tokens == 0)
        {
            light.m_output_reserve_tokens = config.m_model_main.m_output_reserve_tokens;
        }
        return std::make_unique<CModelOpenAICompat>(to_endpoint(light),
                                                    k_default_temperature, output_token_limit(light));
    }

    return std::make_unique<CModelOpenAICompat>(to_endpoint(config.m_model_main),
                                                k_default_temperature, output_token_limit(config.m_model_main));
}

} // namespace fcpp

