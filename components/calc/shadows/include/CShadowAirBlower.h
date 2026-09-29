//
// Создано по образцу CShadowPumpStation.h
//

#ifndef NYM_PROJECT_CSHADOWAIRBLOWER_H
#define NYM_PROJECT_CSHADOWAIRBLOWER_H

#include "../../IShadow.h"
#include <optional>

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

/** @brief Тень воздуходувки для обратной промывки. Опрашивает фильтры ниже по потоку
 *  (тот же приём, что у насоса/ёмкости — каст на конкретную тень, суммирование площадей,
 *  если один узел обслуживает несколько фильтров через общий коллектор). */
class CShadowAirBlower : public IShadow
{
public:
    CShadowAirBlower() = delete;
    explicit CShadowAirBlower(NCore::NComponent * component);
    ~CShadowAirBlower() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SComponentProxy & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                               const Tstring &section_number, uint32_t & formula_start) override;

private:
    struct SLastCalculation
    {
        bool  has_data{false};
        float total_area{0.f};
        float intensity{0.f};
        float pressure{0.f};
        float efficiency{0.f};
        float Qair{0.f};
        float power{0.f};
        float selected_pressure{0.f};
    } m_last_calculation;

    /** @brief nullopt — хоть один фильтр ниже по потоку ещё не подобран, ждём следующего
     *  цикла. 0.f, если ни одного фильтра не найдено вообще (не тот же случай). */
    [[nodiscard]] std::optional<float> sum_downstream_filter_area() const;
};

#endif //NYM_PROJECT_CSHADOWAIRBLOWER_H
