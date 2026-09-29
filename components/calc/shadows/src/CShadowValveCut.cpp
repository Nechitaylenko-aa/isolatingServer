//
// Создано по образцу CShadowThreeWayValve.cpp
//

#include "../include/CShadowValveCut.h"
#include "CParameter.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"
#include "CConductor.h"
#include "CCap.h"

CShadowValveCut::CShadowValveCut(NCore::NComponent *component) : IShadow(component) {}

NCore::SEquipmentRequest
CShadowValveCut::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    // TODO(отложено): нужен диаметр присоединённой трубы (DN), но у CConductor пока
    // нет diameter() — специально НЕ добавляем, чтобы не мешать параллельной работе
    // над тенью трубы (обсуждено отдельно, вернёмся к этому позже).
    // auto *conductor = m_component->input(0)->get_conductor(m_component);
    // if (!conductor) return {};
    // auto dn = conductor->diameter();
    // if (!dn) return {};

    float Pmax_pa = body->get_si_pressure();
    constexpr float PN_MARGIN = 1.5f;  // запас, не выбор — формула технолога

    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;
    req.params = {
            0.f,                            // [0] м — DN (заглушка, см. TODO выше)
            Pmax_pa * PN_MARGIN,             // [1] Па — требуемый PN
            static_cast<float>(m_component->get_project_type()), // [2] тип проекта -> материал/класс герметичности
    };

    return req;
}

std::vector<float>
CShadowValveCut::getCalculationsWithEquip(NCore::SComponentProxy &equip_proxy, NCore::COperatingBody *body,
                                          IGeneralTor *tor)
{
    std::vector<float> res;

    if (equip_proxy.params.size() < 2)
    {
        fprintf(stderr, "CShadowValveCut: received less than 2 equipment fields\n");
        return res;
    }

    float DN = equip_proxy.params.at(0);
    float PN = equip_proxy.params.at(1);

    res.push_back(DN);
    res.push_back(PN);

    m_last_calculation.has_data = true;
    m_last_calculation.DN = DN;
    m_last_calculation.PN = PN;

    // Клапан не меняет тело содержательно.
    return res;
}

std::vector<SReportEntry> CShadowValveCut::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
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
    heading.text = section_number + " Подбор отсечного клапана " + m_component->schematicName();
    report.push_back(heading);

    SReportEntry e;
    e.kind = EReportEntryKind::Formula;
    e.parameter_name = "Номинальное давление клапана";
    e.formula_symbolic = "PN >= Pmax * 1.5";
    e.substituted = "PN = " + fmt(m_last_calculation.PN);
    e.result = m_last_calculation.PN;
    e.unit = "Па";
    e.formula_number = formula_start++;
    report.push_back(e);

    return report;
}
