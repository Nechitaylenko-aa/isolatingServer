//
// Создано по образцу CShadowPumpStation.cpp
//

#include "../include/CShadowAirBlower.h"
//#include "CParameter.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"
//#include "../../IShadowManager.h"
//#include "../../../../automation/IInstallationTemplate.h"
#include "../include/CShadowLightFiletr.h"

CShadowAirBlower::CShadowAirBlower(NCore::NComponent *component) : IShadow(component) {}

std::optional<float> CShadowAirBlower::sum_downstream_filter_area() const
{
    auto predicate = [](NCore::NComponent *c) -> bool
    {
        auto type = c->get_subtype();
        auto *water_type = std::get_if<NCore::E_WATER_COMPONENTS>(&type);
        return water_type && *water_type == NCore::EWB_WATER_FILTER_LIGHT;
    };

    std::vector<NCore::CCell*> visited;
    auto neighbors = NCore::IInstallationTemplate::find_components_downstream_matching(
            m_component->output(0), predicate, visited);

    if (neighbors.empty())
    {
        return 0.f;  // ни одного фильтра ниже по потоку — не тот же случай, что "не готов"
    }

    float total = 0.f;
    for (auto *neighbor : neighbors)
    {
        auto *shadow = IShadowManager::getComponentShadow(neighbor);
        if (!shadow)
        {
            return std::nullopt;
        }

        auto v = dynamic_cast<CShadowLightFilter*>(shadow)->filter_area();
        if (!v)
        {
            return std::nullopt;  // фильтр найден, но ещё не подобран — ждём
        }
        total += *v;
    }

    return total;
}

NCore::SEquipmentRequest
CShadowAirBlower::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    auto area = sum_downstream_filter_area();
    if (!area || *area <= 0.f)
    {
        return {};  // либо фильтр не готов, либо обслуживать вообще нечего
    }

    constexpr float INTENSITY_L_S_M2 = 55.0f;   // TODO(нет данных): середина 50-60 л/(с·м²)
    constexpr float PRESSURE_BAR = 0.5f;        // TODO(нет данных): середина 0.4-0.6 бар,
                                                 // без учёта высоты слоя загрузки (см. чат)
    constexpr float PA_PER_BAR = 100000.0f;

    float Qair_l_s = INTENSITY_L_S_M2 * (*area);
    float Qair_m3_h = Qair_l_s * 3.6f;  // л/с -> м³/ч
    float pressure_pa = PRESSURE_BAR * PA_PER_BAR;

    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;
    req.params = {
            Qair_m3_h,   // [0] м³/ч — расход воздуха, критерий подбора
            pressure_pa, // [1] Па — требуемое давление
    };

    return req;
}

std::vector<float>
CShadowAirBlower::getCalculationsWithEquip(NCore::SEquipLight &equip_proxy, NCore::COperatingBody *body,
                                            IGeneralTor *tor)
{
    std::vector<float> res;

    if (equip_proxy.params.empty())
    {
        fprintf(stderr, "CShadowAirBlower: received 0 equipment fields\n");
        return res;
    }

    float selected_pressure = equip_proxy.params.at(0);  // Па — паспортное давление выбранной установки
    res.push_back(selected_pressure);

    auto area = sum_downstream_filter_area();
    constexpr float INTENSITY_L_S_M2 = 55.0f;
    constexpr float ETA = 0.7f;  // TODO(нет данных): середина 0.6-0.8 для Roots

    float Qair_m3_h = area ? INTENSITY_L_S_M2 * (*area) * 3.6f : 0.f;
    float Qair_m3_s = Qair_m3_h / 3600.0f;
    float power_w = (Qair_m3_s * selected_pressure) / ETA;

    m_last_calculation.has_data = true;
    m_last_calculation.total_area = area ? *area : 0.f;
    m_last_calculation.intensity = INTENSITY_L_S_M2;
    m_last_calculation.pressure = selected_pressure;
    m_last_calculation.efficiency = ETA;
    m_last_calculation.Qair = Qair_m3_h;
    m_last_calculation.power = power_w;
    m_last_calculation.selected_pressure = selected_pressure;

    return res;
}

std::vector<SReportEntry> CShadowAirBlower::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
                                                            const Tstring &section_number, uint32_t &formula_start)
{
    std::vector<SReportEntry> report;

    if (!m_last_calculation.has_data)
    {
        return report;
    }

    auto fmt = [](float v) -> Tstring
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", v);
        return {buf};
    };

    SReportEntry heading;
    heading.kind = EReportEntryKind::SectionHeading;
    heading.text = section_number + " Подбор воздуходувки " + m_component->schematicName();
    report.push_back(heading);

    SReportEntry e1;
    e1.kind = EReportEntryKind::Formula;
    e1.parameter_name = "Расход воздуха на промывку";
    e1.formula_symbolic = "Q = i * F";
    e1.substituted = "Q = " + fmt(m_last_calculation.intensity) + " * " + fmt(m_last_calculation.total_area);
    e1.result = m_last_calculation.Qair;
    e1.unit = "м3/ч";
    e1.formula_number = formula_start++;
    report.push_back(e1);

    SReportEntry e2;
    e2.kind = EReportEntryKind::Formula;
    e2.parameter_name = "Мощность воздуходувки";
    e2.formula_symbolic = "N = (Q * P) / eta";
    e2.substituted = "N = (" + fmt(m_last_calculation.Qair) + " * " + fmt(m_last_calculation.pressure)
                    + ") / " + fmt(m_last_calculation.efficiency);
    e2.result = m_last_calculation.power;
    e2.unit = "Вт";
    e2.formula_number = formula_start++;
    report.push_back(e2);

    return report;
}
