//
// Created by artem on 16.06.26.
//

#include "IShadowManager.h"
#include "COperatingBody.h"
#include "IGeneralTor.h"
#include "IShadow.h"
#include "NComponent.h"
#include "shadows/include/CShadowLightFiletr.h"

IShadowManager::IShadowManager() = default;

IShadowManager &IShadowManager::instance()
{
    static IShadowManager manager;
    return manager;
}

IShadowManager::~IShadowManager()
{
    for (auto &item : m_shadows)
    {
        delete item;
    }
}

NCore::SEquipmentRequest
IShadowManager::getEquipRequest(NCore::NComponent *component, IGeneralTor *tor, NCore::COperatingBody *body)
{
    auto & inst = IShadowManager::instance();
    auto shadow = inst.getShadow(component);

    NCore::SEquipmentRequest request;

    if (shadow)
    {
        request = shadow->getEquipRequest(body, tor);
    }

    return request;
}

std::vector<float>
IShadowManager::getBodyParams(NCore::NComponent *component, NCore::SEquipLight &equipProxy, IGeneralTor *tor, NCore::COperatingBody *body)
{
    std::vector<float> result;

    auto shadow = instance().getShadow(component);
    if (shadow)
    {
        return shadow->getCalculationsWithEquip(equipProxy, body, tor);
    }

    return {};
}

void IShadowManager::generateReport(NCore::NComponent *component, IGeneralTor *tor, NCore::COperatingBody *body,
                                    EReportAction action)
{

}

IShadow *IShadowManager::getShadow(NCore::NComponent *component)
{
    auto iter = std::find_if(m_shadows.begin(), m_shadows.end(),
                             [component](const IShadow* item){ return item->component() == component; });
    if (iter != m_shadows.end())
    {
        return *iter;
    }

    auto type = component->get_subtype();

    IShadow * result = nullptr;

    std::visit([component, &result, this](auto&& arg)
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NCore::E_WATER_COMPONENTS>)
        {
            result = getWaterShadow(arg, component);
        }
        else if constexpr (std::is_same_v<T, NCore::E_GAS_COMPONENTS>)
        {
            result = getGasShadow(arg, component);
        }
        else if constexpr (std::is_same_v<T, NCore::E_ELECTRIC_COMPONENTS>)
        {
            result = getElectricShadow(arg, component);
        }
    }, type);

    if (result)
    {
        m_shadows.push_back(result);
    }

    return result;
}

IShadow *IShadowManager::getWaterShadow(NCore::E_WATER_COMPONENTS subtype, NCore::NComponent *component)
{
    switch (subtype)
    {
        case NCore::E_WATER_COMPONENTS::EWB_WATER_FILTER_LIGHT:
            return new CShadowLightFilter(component);
        case NCore::EWB_WATER_FILTER_ION_EXCHANGE:
            break;
        case NCore::EWB_WATER_FILTER_SORPTION:
            break;
        case NCore::EWB_MEMBRANE_OSMOS:
            break;
        case NCore::EWB_MEMBRANE_NANO:
            break;
        case NCore::EWB_MEMBRANE_ULTRA:
            break;
        case NCore::EWB_CAPACITY:
            break;
        case NCore::EWB_PUMP_STATION:
            break;
        case NCore::EWB_ACCOUNT_NODE:
            break;
        case NCore::EWB_COUNT:
            break;
        default:
            return nullptr;
    }
    return nullptr;
}

IShadow *IShadowManager::getGasShadow(NCore::E_GAS_COMPONENTS subtype, NCore::NComponent *component)
{
    return nullptr;
}

IShadow *IShadowManager::getElectricShadow(NCore::E_ELECTRIC_COMPONENTS subtype, NCore::NComponent *component)
{
    return nullptr;
}
