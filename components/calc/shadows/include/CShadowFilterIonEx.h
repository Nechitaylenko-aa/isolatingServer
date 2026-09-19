//
// Создано по образцу CShadowLightFiletr.h
//

#ifndef NYM_PROJECT_CSHADOWFILTERIONEX_H
#define NYM_PROJECT_CSHADOWFILTERIONEX_H

#include "../../IShadow.h"
#include <map>
#include <optional>

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

class CParameter;
class CLimits;

/** @brief Тень фильтра ионного обмена (умягчение) — свой параметр: жёсткость (EMU_HARDNESS).
 *  Реагентной подготовки перед фильтром не требует (регенерация смолы солевым раствором —
 *  расходник самой этой единицы оборудования, не запрос станции на отдельный узел, см. чат). */
class CShadowFilterIonEx : public IShadow
{
public:
    CShadowFilterIonEx() = delete;
    explicit CShadowFilterIonEx(NCore::NComponent * component);
    ~CShadowFilterIonEx() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                              const Tstring &section_number, uint32_t & formula_start) override;

    /** @brief Объём воды на отмывку смолы после регенерации, в сутки (V = q_уд * W_ионита * n_реген,
     *  формула технолога). q_уд и n_реген — плейсхолдеры в допустимых диапазонах, не расчётные
     *  значения (см. .cpp). nullopt, пока оборудование не подобрано. */
    [[nodiscard]] std::optional<float> regeneration_rinse_volume_required() const;

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
        bool    has_hardness{false};
        float   hardness_in{0.f};
        float   hardness_out{0.f};
        float   efficiency{0.f};
        Tstring hardness_unit;
        float   resin_volume{0.f};  // W_ионита, м³ — из equip_proxy.params[0]
    } m_last_calculation;

    [[nodiscard]] std::map<E_MEASURE_UNITS, SParamWithLimit> collect_tracked_params(
            NCore::COperatingBody * body, IGeneralTor * tor) const;
};

#endif //NYM_PROJECT_CSHADOWFILTERIONEX_H
