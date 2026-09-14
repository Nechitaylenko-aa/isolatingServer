#include "bootstrap.h"
#include "../matchers/CWaterFilterLightHandler.h"
#include "../matchers/CWaterFilterIonExchangeHandler.h"
#include "../matchers/CWaterFilterSorptionHandler.h"
#include "../matchers/CWaterMembarneOsmos.h"
#include "../matchers/CWaterMembraneNano.h"
#include "../matchers/CWaterMembraneUltra.h"
#include "../matchers/CWaterCapacity.h"
#include "../matchers/CWaterPumpStation.h"
#include "../matchers/CWaterAccountNode.h"

void registerEquipHandlers(CEquipHandlerRegistry& registry)
{
    registry.registerHandler({NCore::ect_water, NCore::EWB_WATER_FILTER_LIGHT},
                             std::make_unique<CWaterFilterLightHandler>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_WATER_FILTER_ION_EXCHANGE},
                             std::make_unique<CWaterFilterIonExchangeHandler>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_WATER_FILTER_SORPTION},
                             std::make_unique<CWaterFilterSorptionHandler>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_MEMBRANE_OSMOS},
                             std::make_unique<CWaterMembarneOsmos>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_MEMBRANE_NANO},
                             std::make_unique<CWaterMembraneNano>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_MEMBRANE_ULTRA},
                             std::make_unique<CWaterMembraneUltra>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_CAPACITY},
                             std::make_unique<CWaterCapacity>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_PUMP_STATION},
                             std::make_unique<CWaterPumpStation>());

    registry.registerHandler({NCore::ect_water, NCore::EWB_ACCOUNT_NODE},
                             std::make_unique<CWaterAccountNode>());
}
