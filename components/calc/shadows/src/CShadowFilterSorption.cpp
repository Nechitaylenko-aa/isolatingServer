//
// Создано по образцу CShadowLightFiletr.cpp
//

#include "../include/CShadowFilterSorption.h"
#include "CParameter.h"
#include "CLimits.h"
#include "Logger.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"

CShadowFilterSorption::CShadowFilterSorption(NCore::NComponent *component) : IShadow(component) {}

std::map<E_MEASURE_UNITS, CShadowFilterSorption::SParamWithLimit>
CShadowFilterSorption::collect_tracked_params(NCore::COperatingBody *body, IGeneralTor *tor) const
{
    // свои параметры сорбционного фильтра — запах и вкус (не мутность/цветность, это CShadowLightFilter)
    std::map<E_MEASURE_UNITS, SParamWithLimit> tracked {
            {EMU_SMELL,  {}},
            {EMU_FLAVOR, {}},
    };

    for (uint32_t i = 0; i < body->parameters_count(); ++i)
    {
        auto param = body->get_parameter(i);
        E_MEASURE_UNITS measure_unit = param->measure_unit()->measure_unit();

        auto it = tracked.find(measure_unit);
        if (it == tracked.end())
        {
            continue;
        }

        it->second.param = param;
        auto range = tor->limit_at(i);
        if (range && range->limits() && !range->limits()->empty())
        {
            it->second.limits = range;
        }
    }

    return tracked;
}

NCore::SEquipmentRequest
CShadowFilterSorption::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    auto tracked = collect_tracked_params(body, tor);

    float m_Q = tor->hourInputMax()->si_value();  // м³/ч
    float m_pressure_pa = body->get_si_pressure();
    float temp = body->get_si_temperature();      // °C

    bool is_work = false;
    for (const auto &kv : tracked)
    {
        if (kv.second.limits)
        {
            is_work = true;
            break;
        }
    }

    if (!is_work)
    {
        return {};
    }

    // ВНИМАНИЕ, открытый вопрос (не решён, не гадаю): в отличие от осветлительного фильтра,
    // здесь НЕТ SReagentRequirement. Предполагаю, что запах/вкус снимаются самой угольной загрузкой
    // (адсорбция), без реагентной подготовки перед фильтром — в отличие от коагулянта для
    // мутности/цветности. Если на практике для сильного запаха всё же нужна доокислительная
    // подготовка (озон/перманганат) до угля — это отдельное решение технолога, сюда не добавлено.

    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;
    req.params = {
            m_Q,           // [0] м³/ч - расход
            m_pressure_pa, // [1] Па - рабочее давление
            temp,          // [2] °C - температура
    };

    return req;
}

std::vector<float>
CShadowFilterSorption::getCalculationsWithEquip(NCore::SEquipLight &equip_proxy, NCore::COperatingBody *body,
                                                 IGeneralTor *tor)
{
    std::vector<float> res;

    auto tracked = collect_tracked_params(body, tor);
    const auto &smell_entry  = tracked[EMU_SMELL];
    const auto &flavor_entry = tracked[EMU_FLAVOR];

    if (!smell_entry.param && !flavor_entry.param)
    {
        return res;
    }

    if (equip_proxy.params.size() < 1)
    {
        fprintf(stderr, "CShadowFilterSorption: received 0 equipment fields\n");
        return res;
    }

    // ЗАГЛУШКА: реальной кинетики адсорбции (время контакта/пробойный график угля) нет —
    // не выдумываю коэффициенты по образцу CShadowLightFilter, там они хотя бы обсуждались
    // отдельно. Здесь эффективность — единственная константа-плейсхолдер до того, как появится
    // формула от технолога.
    float efficiency = 0.6f;

    float smell_out  = smell_entry.param  ? smell_entry.param->si_value()  * (1.0f - efficiency) : 0.f;
    float flavor_out = flavor_entry.param ? flavor_entry.param->si_value() * (1.0f - efficiency) : 0.f;

    res.push_back(smell_out);
    res.push_back(flavor_out);

    m_last_calculation.has_data = true;
    m_last_calculation.has_smell = (smell_entry.param != nullptr);
    m_last_calculation.has_flavor = (flavor_entry.param != nullptr);
    m_last_calculation.smell_in = smell_entry.param ? smell_entry.param->si_value() : 0.f;
    m_last_calculation.smell_out = smell_out;
    m_last_calculation.flavor_in = flavor_entry.param ? flavor_entry.param->si_value() : 0.f;
    m_last_calculation.flavor_out = flavor_out;
    m_last_calculation.efficiency = efficiency;
    m_last_calculation.smell_unit = smell_entry.param ? smell_entry.param->dimension_si_name() : "";
    m_last_calculation.flavor_unit = flavor_entry.param ? flavor_entry.param->dimension_si_name() : "";

    return res;
}

std::vector<SReportEntry> CShadowFilterSorption::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
                                                  const Tstring &section_number, uint32_t & formula_start)
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

    uint32_t formula_no = formula_start;

    SReportEntry heading;
    heading.kind = EReportEntryKind::SectionHeading;
    heading.text = section_number + " Расчет фильтра сорбционного " + m_component->schematicName();
    report.push_back(heading);

    if (m_last_calculation.has_smell)
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Formula;
        e.parameter_name = "Запах на выходе";
        e.formula_symbolic = "C_out = C_in * (1 - eta)";
        e.substituted = "C_out = " + fmt(m_last_calculation.smell_in) + " * (1 - "
                       + fmt(m_last_calculation.efficiency) + ")";
        e.result = m_last_calculation.smell_out;
        e.unit = m_last_calculation.smell_unit;
        e.formula_number = formula_no++;
        report.push_back(e);
    }

    if (m_last_calculation.has_flavor)
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Formula;
        e.parameter_name = "Привкус на выходе";
        e.formula_symbolic = "C_out = C_in * (1 - eta)";
        e.substituted = "C_out = " + fmt(m_last_calculation.flavor_in) + " * (1 - "
                       + fmt(m_last_calculation.efficiency) + ")";
        e.result = m_last_calculation.flavor_out;
        e.unit = m_last_calculation.flavor_unit;
        e.formula_number = formula_no++;
        report.push_back(e);
    }

    return report;
}
