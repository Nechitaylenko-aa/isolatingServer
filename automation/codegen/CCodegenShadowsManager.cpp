//
// Created by artem on 25.08.26.
//

#include "CCodegenShadowsManager.h"
#include "NComponent.h"
#include "shadows/include/CShadowPumpStation.h"
#include "shadows/include/CShadowAirBlower.h"

namespace NCore
{
    ICodegenShadow *CCodegenShadowsManager::codegen_shadow_instance(NComponent *owner)
    {
        auto nature = (E_BODY_TYPE)owner->get_subtype().index();

        switch (nature)
        {
            case 0:
                return get_water_codegen(owner);
            case BT_GAS:
            case BT_ELECTRICITY:
                break;
            default:
                return nullptr;
        }
        return nullptr;
    }

    ICodegenShadow *CCodegenShadowsManager::get_water_codegen(NComponent *owner)
    {
        E_WATER_COMPONENTS component = (E_WATER_COMPONENTS)ComponentConverter::get_int_TComponentType(owner->get_subtype());

        switch (component)
        {
            case EWB_WATER_FILTER_LIGHT:
            case EWB_WATER_FILTER_ION_EXCHANGE:
            case EWB_WATER_FILTER_SORPTION:
            case EWB_MEMBRANE_OSMOS:
            case EWB_MEMBRANE_NANO:
            case EWB_MEMBRANE_ULTRA:
            case EWB_CAPACITY:
                break;
            case EWB_PUMP_STATION:
                return new CShadowPumpStation(owner);
            case EWB_AIR_BLOWER:
                return new CShadowAirBlower(owner);
            case EWB_ACCOUNT_NODE:
            case EWB_VALVE_WATER_THREE_WAY:
            default:
                return nullptr;
        }
        return nullptr;
    }
} // NCore
