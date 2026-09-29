//
// Создано по образцу CShadowPumpStation.cpp
//

#include "../include/CShadowThreeWayValve.h"
#include "CParameter.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"
#include <cmath>

CShadowThreeWayValve::CShadowThreeWayValve(NCore::NComponent *component) : IShadow(component) {}

NCore::SEquipmentRequest
CShadowThreeWayValve::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    float Qmax = tor->hourInputMax()->si_value();  // м³/ч
    float Pmax_pa = body->get_si_pressure();        // Па — рабочее давление после насоса

    constexpr float DELTA_P_BAR = 0.2f;  // TODO(нет данных): допустимые потери, пример технолога

    float Kv = Qmax / std::sqrt(DELTA_P_BAR);

    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;
    req.params = {
            Kv,        // [0] требуемый Kv — основной критерий подбора, не DN (см. документ технолога)
            Pmax_pa,   // [1] Па — определяет класс давления (PN)
    };

    return req;
}

std::vector<float>
CShadowThreeWayValve::getCalculationsWithEquip(NCore::SComponentProxy &equip_proxy, NCore::COperatingBody *body,
                                               IGeneralTor *tor)
{
    std::vector<float> res;

    if (equip_proxy.params.size() < 2)
    {
        fprintf(stderr, "CShadowThreeWayValve: received less than 2 equipment fields\n");
        return res;
    }

    float DN = equip_proxy.params.at(0);
    float Kv_actual = equip_proxy.params.at(1);  // паспортный Kv выбранной модели, не то, что просили

    res.push_back(DN);
    res.push_back(Kv_actual);

    m_last_calculation.has_data = true;
    m_last_calculation.Qmax = tor->hourInputMax()->si_value();
    m_last_calculation.delta_p = 0.2f;
    m_last_calculation.Kv = Kv_actual;
    m_last_calculation.DN = DN;

    // Клапан не меняет тело содержательно (проход/переключение — automation-слой, не calc).
    return res;
}

std::vector<SReportEntry> CShadowThreeWayValve::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
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
        return Tstring(buf);
    };

    SReportEntry heading;
    heading.kind = EReportEntryKind::SectionHeading;
    heading.text = section_number + " Подбор трёхходового клапана " + m_component->schematicName();
    report.push_back(heading);

    SReportEntry e;
    e.kind = EReportEntryKind::Formula;
    e.parameter_name = "Требуемый Kv";
    e.formula_symbolic = "Kv = Qmax / sqrt(dP)";
    e.substituted = "Kv = " + fmt(m_last_calculation.Qmax) + " / sqrt(" + fmt(m_last_calculation.delta_p) + ")";
    e.result = m_last_calculation.Kv;
    e.unit = "";
    e.formula_number = formula_start++;
    report.push_back(e);

    return report;
}
