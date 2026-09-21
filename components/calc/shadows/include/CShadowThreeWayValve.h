//
// Создано по образцу CShadowPumpStation.h
//

#ifndef NYM_PROJECT_CSHADOWTHREEWAYVALVE_H
#define NYM_PROJECT_CSHADOWTHREEWAYVALVE_H

#include "../../IShadow.h"

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

/** @brief Тень трёхходового клапана-переключателя. По документу технолога: Q -> Kv -> DN,
 *  остальное (тип, привод, питание, концевики) — фиксированная спецификация линейки,
 *  не критерий подбора, просто передаётся/подразумевается как признак каталожной позиции. */
class CShadowThreeWayValve : public IShadow
{
public:
    CShadowThreeWayValve() = delete;
    explicit CShadowThreeWayValve(NCore::NComponent * component);
    ~CShadowThreeWayValve() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                               const Tstring &section_number, uint32_t & formula_start) override;

private:
    struct SLastCalculation
    {
        bool  has_data{false};
        float Qmax{0.f};
        float delta_p{0.f};
        float Kv{0.f};
        float DN{0.f};
    } m_last_calculation;
};

#endif //NYM_PROJECT_CSHADOWTHREEWAYVALVE_H
