/**
 * @file CModelLevels.cpp
 * @brief Shared model availability and effective endpoint identity.
 */
#include "core/CModelLevels.h"

#include <memory>
#include <string>
#include <vector>

#include "fcpp-agent/IModelApi.h"
#include "llm/CModelFactory.h"

namespace fcpp
{

EModelRoleKind model_role_for_level(EModelLevel level)
{
    switch (level)
    {
        case EModelLevel::Lite: return EModelRoleKind::Light;
        case EModelLevel::Main: return EModelRoleKind::Main;
        case EModelLevel::Remote: return EModelRoleKind::Remote;
        case EModelLevel::Count: break;
    }
    return EModelRoleKind::Main;
}

std::string model_endpoint_signature(const SConfig& config, EModelLevel level)
{
    const SModelConfig* settings = &config.m_model_main;
    if (level == EModelLevel::Lite)
    {
        settings = &config.m_model_light;
    }
    else if (level == EModelLevel::Remote)
    {
        settings = &config.m_model_remote;
    }

    std::string url = settings->m_url;
    std::string name = settings->m_name;
    if (level == EModelLevel::Lite)
    {
        // Mirror CModelFactory::create exactly: URL inheritance does not overwrite
        // an explicitly configured light model name.
        if (url.empty() || url == "same_as_main")
        {
            url = config.m_model_main.m_url;
        }
        if (name.empty())
        {
            name = config.m_model_main.m_name;
        }
    }

    const std::string provider = level == EModelLevel::Remote
        && config.m_remote_type == EModelRemoteType::Claude
            ? "claude" : "openai-compatible";
    return url + "|" + name + "|" + provider;
}

std::vector<EModelLevel> available_model_levels(const SConfig& config)
{
    const EModelLevel candidates[] = {
        EModelLevel::Lite, EModelLevel::Main, EModelLevel::Remote
    };
    std::vector<EModelLevel> levels;
    std::vector<std::string> signatures;

    for (const EModelLevel level : candidates)
    {
        std::unique_ptr<IModelApi> model = CModelFactory::create(config, model_role_for_level(level));
        if (model == nullptr || !model->is_available())
        {
            continue;
        }

        const std::string signature = model_endpoint_signature(config, level);
        bool duplicate = false;
        for (const std::string& known : signatures)
        {
            if (known == signature)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            levels.push_back(level);
            signatures.push_back(signature);
        }
    }
    return levels;
}

} // namespace fcpp
