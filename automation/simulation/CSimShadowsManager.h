//
// Created by artem on 28.08.26.
//

#ifndef NYM_PROJECT_CSIMSHADOWSMANAGER_H
#define NYM_PROJECT_CSIMSHADOWSMANAGER_H

#include "shadows/include/ISimulationShadow.h"

namespace NCore{

    class CSimShadowsManager
    {
    public:
        static ISimulationShadow* instance(NComponent *component);
    private:
        static ISimulationShadow* water_shadow(NComponent *component, E_WATER_COMPONENTS type);
        static ISimulationShadow* gas_shadow(NComponent *component, E_GAS_COMPONENTS type);
        static ISimulationShadow* electric_shadow(NComponent *component, E_ELECTRIC_COMPONENTS type);
    };

}


#endif //NYM_PROJECT_CSIMSHADOWSMANAGER_H
