//
// Создано по образцу CShadowLightFiletr.h
//

#ifndef NYM_PROJECT_CSHADOWFILTERSORPTION_H
#define NYM_PROJECT_CSHADOWFILTERSORPTION_H

#include "../../IShadow.h"
#include <map>

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

class CParameter;
class CLimits;

/** @brief Тень сорбционного фильтра (активированный уголь) — свои параметры: запах и вкус
 *  (мутность/цветность — зона ответственности CShadowLightFilter, см. чат). */
class CShadowFilterSorption : public IShadow
{
public:
    CShadowFilterSorption() = delete;
    explicit CShadowFilterSorption(NCore::NComponent * component);
    ~CShadowFilterSorption() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                              const Tstring &section_number, uint32_t & formula_start) override;

private:
    struct SParamWithLimit
    {
        CParameter * param{nullptr};
        CLimits    * limits{nullptr};
    };

    /** @brief снимок последнего расчёта — см. аналогичный комментарий в CShadowLightFilter.h */
    struct SLastCalculation
    {
        bool    has_data{false};
        bool    has_smell{false};
        bool    has_flavor{false};
        float   smell_in{0.f};
        float   smell_out{0.f};
        float   flavor_in{0.f};
        float   flavor_out{0.f};
        float   efficiency{0.f};
        Tstring smell_unit;
        Tstring flavor_unit;
    } m_last_calculation;

    [[nodiscard]] std::map<E_MEASURE_UNITS, SParamWithLimit> collect_tracked_params(
            NCore::COperatingBody * body, IGeneralTor * tor) const;
};

#endif //NYM_PROJECT_CSHADOWFILTERSORPTION_H
