//
// Created by artem on 16.06.26.
//

#ifndef NYM_PROJECT_CSHADOWLIGHTFILETR_H
#define NYM_PROJECT_CSHADOWLIGHTFILETR_H

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

class CShadowLightFilter : public IShadow
{
public:
    CShadowLightFilter() = delete;
    explicit CShadowLightFilter(NCore::NComponent * component);
    ~CShadowLightFilter() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                              const Tstring &section_number, uint32_t &formula_start) override;

private:
    struct SParamWithLimit
    {
        CParameter * param{nullptr};
        CLimits    * limits{nullptr};
    };

    /** @brief единственная точка сканирования параметров тела на мутность/цветность + их лимитов
     *  (запах/вкус — вне зоны ответственности этого фильтра, см. чат).
     *  Используется и в getEquipRequest, и в getCalculationsWithEquip — раньше это был скопированный
     *  дважды цикл, который неизбежно разошёлся бы в третий раз в generateReport(). */
    /** @brief снимок последнего расчёта — чтобы generateReport() не пересчитывал efficiency заново
     *  (та же формула в двух местах неизбежно разошлась бы при следующей правке), а просто
     *  отформатировал уже посчитанное. Заполняется в конце getCalculationsWithEquip(). */
    struct SLastCalculation
    {
        bool    has_data{false};
        bool    has_turbidity{false};
        bool    has_chromaticity{false};
        float   turbidity_in{0.f};
        float   turbidity_out{0.f};
        float   chromaticity_in{0.f};
        float   chromaticity_out{0.f};
        float   efficiency{0.f};
        Tstring turbidity_unit;
        Tstring chromaticity_unit;
        float   filter_area{0.f};          // F, м² — площадь фильтрования, нужна для объёма промывки
        bool    has_backwash_intensity{false};
        float   backwash_intensity{0.f};   // i, л/(с·м²) — известна не для всех типов загрузки, см. .cpp
        float   v_max{0.f};                 // м/ч — из выбранного оборудования, нужен насосу
    } m_last_calculation;

    [[nodiscard]] std::map<E_MEASURE_UNITS, SParamWithLimit> collect_tracked_params(
            NCore::COperatingBody * body, IGeneralTor * tor) const;

public:
    /** @brief Объём воды на одну обратную промывку (V = 0.06 * i * t * F, t = 20 мин фиксировано —
     *  оба значения от технолога, см. чат). nullopt, если оборудование ещё не подобрано ИЛИ если
     *  для выбранного типа загрузки интенсивность промывки неизвестна (сейчас есть только для
     *  антрацита — 10-12 л/(с·м²); для керамзита/цеолита технолог значений не давал, вкв.
     *  песок в EFilterMediaType вообще отсутствует, хотя формула его упоминает). */
    [[nodiscard]] std::optional<float> backwash_volume_required() const;

    /** @brief Предельное давление на входе, которое насос перед этим фильтром не должен превышать
     *  ("не продавить"). Обратная величина того же запаса (10%) и той же гидравлики, что уже
     *  использовались в getCalculationsWithEquip для предупреждения — не дублирую формулу заново,
     *  см. .cpp. nullopt, пока оборудование не подобрано. */
    [[nodiscard]] std::optional<float> max_allowed_pressure_pa() const;
};


#endif //NYM_PROJECT_CSHADOWLIGHTFILETR_H
