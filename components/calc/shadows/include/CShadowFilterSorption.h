//
// Создано по образцу CShadowLightFiletr.h
//

#ifndef NYM_PROJECT_CSHADOWFILTERSORPTION_H
#define NYM_PROJECT_CSHADOWFILTERSORPTION_H

#include "../../IShadow.h"

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

class CShadowFilterSorption : public IShadow
{
public:
    CShadowFilterSorption() = delete;
    explicit CShadowFilterSorption(NCore::NComponent * component);
    ~CShadowFilterSorption() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    void  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action) override;

private:
    NCore::NComponent * m_component;
};

#endif //NYM_PROJECT_CSHADOWFILTERSORPTION_H
