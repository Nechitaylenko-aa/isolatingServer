//
// Created by artem on 16.06.26.
//

#ifndef NYM_PROJECT_ISHADOWMANAGER_H
#define NYM_PROJECT_ISHADOWMANAGER_H

#include "CInfoBus.h"
//#include "CEquipment.h"


class IGeneralTor;

namespace NCore {
    class COperatingBody;
}

class IShadow;

enum EReportAction
{

};

class IShadowManager
{
public:
    IShadowManager(const IShadowManager &) = delete;
    IShadowManager(IShadowManager &&) = delete;
    IShadowManager& operator=(const IShadowManager &) = delete;
    ~IShadowManager();

    static  NCore::SEquipmentRequest  getEquipRequest(NCore::NComponent * component, IGeneralTor * tor, NCore::COperatingBody *body);
    static  std::vector<float>        getBodyParams(NCore::NComponent *component, NCore::SEquipLight &equip_proxy, IGeneralTor * tor, NCore::COperatingBody *body);
    static  void  generateReport(NCore::NComponent * component, IGeneralTor * tor, NCore::COperatingBody *body, EReportAction action);

private:
    IShadowManager();
    static IShadowManager &instance();
    IShadow* getShadow(NCore::NComponent * component);
    std::vector<IShadow*>  m_shadows;

    IShadow* getWaterShadow(NCore::E_WATER_COMPONENTS subtype, NCore::NComponent* component);
    IShadow* getGasShadow(NCore::E_GAS_COMPONENTS subtype, NCore::NComponent* component);
    IShadow* getElectricShadow(NCore::E_ELECTRIC_COMPONENTS subtype, NCore::NComponent* component);
};


#endif //NYM_PROJECT_ISHADOWMANAGER_H
