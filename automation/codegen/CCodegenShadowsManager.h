//
// Created by artem on 25.08.26.
//

#ifndef NYM_PROJECT_CCODEGENSHADOWSMANAGER_H
#define NYM_PROJECT_CCODEGENSHADOWSMANAGER_H

#include "ICodegenShadow.h"

namespace NCore
{

    class CCodegenShadowsManager
    {
    public:
        static ICodegenShadow*   codegen_shadow_instance(NComponent * owner);


        static ICodegenShadow * get_water_codegen(NComponent *owner);
    };

} // NCore

#endif //NYM_PROJECT_CCODEGENSHADOWSMANAGER_H
