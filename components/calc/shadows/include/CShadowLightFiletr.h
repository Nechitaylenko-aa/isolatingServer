//
// Created by artem on 16.06.26.
//

#ifndef NYM_PROJECT_CSHADOWLIGHTFILETR_H
#define NYM_PROJECT_CSHADOWLIGHTFILETR_H

#include "../../IShadow.h"

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

class CShadowLightFilter : public IShadow
{
public:
    CShadowLightFilter() = delete;
    explicit CShadowLightFilter(NCore::NComponent * component);
    ~CShadowLightFilter() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    void  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action) override;

private:
    NCore::NComponent * m_component;
};


#endif //NYM_PROJECT_CSHADOWLIGHTFILETR_H
