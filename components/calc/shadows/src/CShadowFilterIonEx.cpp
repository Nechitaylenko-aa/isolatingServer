//
// Создано по образцу CShadowLightFiletr.cpp
//

#include "../include/CShadowFilterIonEx.h"
#include "CParameter.h"
#include "CLimits.h"
#include "Logger.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"

CShadowFilterIonEx::CShadowFilterIonEx(NCore::NComponent *component) : IShadow(component) {}

std::map<E_MEASURE_UNITS, CShadowFilterIonEx::SParamWithLimit>
CShadowFilterIonEx::collect_tracked_params(NCore::COperatingBody *body, IGeneralTor *tor) const
{
    // свой параметр фильтра ионного обмена — жёсткость (не мутность/цветность/запах/вкус)
    std::map<E_MEASURE_UNITS, SParamWithLimit> tracked {
            {EMU_HARDNESS, {}},
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
CShadowFilterIonEx::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    auto tracked = collect_tracked_params(body, tor);
    const auto &hardness_entry = tracked[EMU_HARDNESS];

    if (!hardness_entry.limits)
    {
        return {};
    }

    float m_Q = tor->hourInputMax()->si_value();  // м³/ч
    float m_pressure_pa = body->get_si_pressure();
    float temp = body->get_si_temperature();      // °C

    // Реагента здесь нет: умягчение снимает саму смолу (катионит), а не подготовка перед
    // фильтром — регенерация солью это расходник самого оборудования, не требование станции.

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
CShadowFilterIonEx::getCalculationsWithEquip(NCore::SEquipLight &equip_proxy, NCore::COperatingBody *body,
                                              IGeneralTor *tor)
{
    std::vector<float> res;

    auto tracked = collect_tracked_params(body, tor);
    const auto &hardness_entry = tracked[EMU_HARDNESS];

    if (!hardness_entry.param)
    {
        return res;
    }

    if (equip_proxy.params.empty())
    {
        fprintf(stderr, "CShadowFilterIonEx: received 0 equipment fields\n");
        return res;
    }
    float resin_volume = equip_proxy.params.at(0);  // м³ — объём загрузки ионита (W_ионита)

    // ЗАГЛУШКА: реальной формулы обменной ёмкости катионита (мг-экв/л по факту загрузки) нет —
    // не выдумываю, как и в CShadowFilterSorption. Одна константа-плейсхолдер до данных технолога.
    float efficiency = 0.9f;  // ионный обмен обычно снимает жёсткость почти полностью, пока смола не истощена

    float hardness_out = hardness_entry.param->si_value() * (1.0f - efficiency);
    res.push_back(hardness_out);

    m_last_calculation.has_data = true;
    m_last_calculation.has_hardness = true;
    m_last_calculation.hardness_in = hardness_entry.param->si_value();
    m_last_calculation.hardness_out = hardness_out;
    m_last_calculation.efficiency = efficiency;
    m_last_calculation.hardness_unit = hardness_entry.param->dimension_si_name();
    m_last_calculation.resin_volume = resin_volume;

    return res;
}

std::optional<float> CShadowFilterIonEx::regeneration_rinse_volume_required() const
{
    if (!m_last_calculation.has_data)
    {
        return std::nullopt;
    }

    // q_уд: технолог дал диапазон 3-4 м³/м³ для катионита/анионита, беру середину — не выбор
    // конкретного значения инженером, а плейсхолдер до его решения.
    constexpr float q_specific = 3.5f;
    // n_реген: технолог дал "не более двух в сутки" — беру максимум как консервативную (худшую)
    // оценку для размера ёмкости, не расчётное значение (расчётное зависит от фактической
    // ёмкости смолы и суточной нагрузки по жёсткости, которых у нас нет).
    constexpr float n_regen_per_day = 2.0f;

    return q_specific * m_last_calculation.resin_volume * n_regen_per_day;
}

std::vector<SReportEntry> CShadowFilterIonEx::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
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
    heading.text = section_number + " Расчет фильтра ионного обмена " + m_component->schematicName();
    report.push_back(heading);

    if (m_last_calculation.has_hardness)
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Formula;
        e.parameter_name = "Жёсткость на выходе";
        e.formula_symbolic = "H_out = H_in * (1 - eta)";
        e.substituted = "H_out = " + fmt(m_last_calculation.hardness_in) + " * (1 - "
                       + fmt(m_last_calculation.efficiency) + ")";
        e.result = m_last_calculation.hardness_out;
        e.unit = m_last_calculation.hardness_unit;
        e.formula_number = formula_no++;
        report.push_back(e);
    }

    if (auto rinse = regeneration_rinse_volume_required())
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Formula;
        e.parameter_name = "Объём воды на отмывку после регенерации (в сутки)";
        e.formula_symbolic = "V = q_specific * W_resin * n_regen";
        e.substituted = "V = 3.5 * " + fmt(m_last_calculation.resin_volume) + " * 2";
        e.result = *rinse;
        e.unit = "м3";
        e.formula_number = formula_no++;
        report.push_back(e);
    }

    return report;
}
