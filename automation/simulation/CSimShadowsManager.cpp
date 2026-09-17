//
// Created by artem on 28.08.26.
//

#include "CSimShadowsManager.h"
#include "NComponent.h"
#include "shadows/include/CSimPumpStation.h"
#include "shadows/include/CSimWaterCapacity.h"
#include "shadows/include/CSimValveTwoDirectional.h"
#include "shadows/include/CSimValveCut.h"
#include "shadows/include/CSimAccountNode.h"
#include "shadows/include/CSimFilterSorption.h"
#include "shadows/include/CSimLightFilter.h"
#include "shadows/include/CSimAirBlower.h"
#include "shadows/include/CSimUFLamp.h"

namespace NCore
{


    ISimulationShadow * CSimShadowsManager::instance(NComponent *component)
    {
        int type = ComponentConverter::get_int_TComponentType(component->get_component_type());
        switch (component->get_component_type().index())
        {
            case 0:
                return water_shadow(component, (E_WATER_COMPONENTS)type);
            case 1:
                return gas_shadow(component, (E_GAS_COMPONENTS)type);
            case 2:
                return electric_shadow(component, (E_ELECTRIC_COMPONENTS)type);
            default:
                return nullptr;
        }
    }

    ISimulationShadow *CSimShadowsManager::water_shadow(NComponent *component, E_WATER_COMPONENTS type)
    {
        switch (type)
        {
            case EWB_CAPACITY:
                return new CSimWaterCapacity(component);
            case EWB_PUMP_STATION:
                return new CSimPumpStation(component);
            case EWB_VALVE_WATER_TWO_WAY:
                return new CSimValveTwoDirectional(component);
            case EWB_WATER_FILTER_LIGHT:
                return new CSimLightFilter(component);
            case EWB_WATER_FILTER_SORPTION:
                return new CSimFilterSorption(component);
            case EWB_ACCOUNT_NODE:
                return new CSimAccountNode(component);
            case EWB_VALVE_CUT:
                return new CSimValveCut(component);
            case EWB_UF_LAMP:
                return new CSimUFLamp(component);
            case EWB_AIR_BLOWER:
                return new CSimAirBlower(component);
            case EWB_WATER_FILTER_ION_EXCHANGE:
            case EWB_MEMBRANE_OSMOS:
            case EWB_MEMBRANE_NANO:
            case EWB_MEMBRANE_ULTRA:
            case EWB_AERATOR:
            case EWB_DOZER:
            case EWB_DOZER_PREP:
            case EWB_PACKET_DEWATER:
            default:
                return nullptr;
        }
    }

    ISimulationShadow *CSimShadowsManager::gas_shadow(NComponent *component, E_GAS_COMPONENTS type)
    {
        return nullptr;
    }

    ISimulationShadow *CSimShadowsManager::electric_shadow(NComponent *component, E_ELECTRIC_COMPONENTS type)
    {
        return nullptr;
    }

}
