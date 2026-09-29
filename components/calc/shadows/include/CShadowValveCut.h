//
// Создано по образцу CShadowThreeWayValve.h
//

#ifndef NYM_PROJECT_CSHADOWVALVECUT_H
#define NYM_PROJECT_CSHADOWVALVECUT_H

#include "../../IShadow.h"

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

/** @brief Тень отсечного клапана. Привод — фиксированно электрический (не критерий
 *  подбора, см. чат). Тип (задвижка/шаровой/дисковый/ножевой), PN и материал решает
 *  каталог по DN + рабочему давлению + типу проекта — здесь только сбор критериев. */
class CShadowValveCut : public IShadow
{
public:
    CShadowValveCut() = delete;
    explicit CShadowValveCut(NCore::NComponent * component);
    ~CShadowValveCut() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SComponentProxy & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                               const Tstring &section_number, uint32_t & formula_start) override;

private:
    struct SLastCalculation
    {
        bool  has_data{false};
        float DN{0.f};
        float PN{0.f};
    } m_last_calculation;
};

#endif //NYM_PROJECT_CSHADOWVALVECUT_H
