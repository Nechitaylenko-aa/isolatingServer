//
// Создано по образцу CShadowWaterCapacity.h
//

#ifndef NYM_PROJECT_CSHADOWPUMPSTATION_H
#define NYM_PROJECT_CSHADOWPUMPSTATION_H

#include "../../IShadow.h"
#include <optional>

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

/** @brief Тень насосной станции — сама не решает "что" стоит следующим, только опрашивает уже
 *  подобранных соседей ниже по потоку (см. чат): ёмкость — нужный напор, чтобы налить доверху
 *  (максимум по всем найденным, если их несколько); фильтр — предел, который нельзя превышать
 *  ("не продавить", минимум по всем найденным). Если требование ёмкости выше предела фильтра —
 *  это конфликт проекта (насос физически не может угодить обоим), не решаю его молча. */
class CShadowPumpStation : public IShadow
{
public:
    CShadowPumpStation() = delete;
    explicit CShadowPumpStation(NCore::NComponent * component);
    ~CShadowPumpStation() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                               const Tstring &section_number, uint32_t & formula_start) override;

private:
    struct SLastCalculation
    {
        bool    has_data{false};
        float   min_required{0.f};   // Па — максимум по ёмкостям ниже по потоку (0, если их нет)
        float   max_allowed{0.f};    // Па — минимум по фильтрам ниже по потоку (нет предела, если их нет)
        bool    has_upper_bound{false};
        float   selected_pressure{0.f};
    } m_last_calculation;

    /** @brief nullopt — сосед найден, но ещё не подобран (жди следующего цикла).
     *  has_upper_bound=false в результате — фильтров ниже по потоку не найдено, предела нет. */
    struct SDownstreamDemand
    {
        float min_required{0.f};
        float max_allowed{0.f};
        bool  has_upper_bound{false};
    };
    [[nodiscard]] std::optional<SDownstreamDemand> gather_downstream_demand() const;
};

#endif //NYM_PROJECT_CSHADOWPUMPSTATION_H
