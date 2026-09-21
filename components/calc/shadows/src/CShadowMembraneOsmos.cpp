#include "../include/CShadowMembraneOsmos.h"
#include "CParameter.h"
#include "CBodyParam.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"

CShadowMembraneOsmos::CShadowMembraneOsmos(NCore::NComponent *component) : IShadow(component) {}

NCore::SEquipmentRequest
CShadowMembraneOsmos::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    return {};  // не подбор из БД — см. заголовок .h
}

std::vector<float>
CShadowMembraneOsmos::getCalculationsWithEquip(NCore::SEquipLight &equip_proxy, NCore::COperatingBody *body, IGeneralTor *tor)
{
    return {};  // не используется, см. .h
}

std::pair<float, float> CShadowMembraneOsmos::purify(NCore::COperatingBody *body, IGeneralTor *tor)
{
    constexpr float RECOVERY = 0.75f;    // TODO(нет данных): 70-85% для солоноватой воды, середина
    constexpr float REJECTION = 0.97f;   // TODO(нет данных): 95-99%, середина

    float Q_in = tor->hourInputMax()->si_value();
    float Q_permeate = Q_in * RECOVERY;
    float Q_concentrate = Q_in * (1.0f - RECOVERY);

    std::vector<std::tuple<Tstring, float, Tstring>> input_composition;
    for (uint32_t i = 0; i < body->parameters_count(); ++i)
    {
        auto *param = body->get_parameter(i);
        if (param->measure_unit()->measure_unit() == EMU_CONCENTRATION)
        {
            input_composition.emplace_back(param->unit_name(), param->si_value(), param->dimension_si_name());
            param->set_si_value(param->si_value() * (1.0f - REJECTION));
        }
    }
    // TODO(нет данных): концентрат сейчас уходит с НЕизменённым (неконцентрированным)
    // составом — честный масс-баланс солей по концентрату не считаю, только объём.

    m_last_calculation.has_data = true;
    m_last_calculation.Q_in = Q_in;
    m_last_calculation.Q_permeate = Q_permeate;
    m_last_calculation.Q_concentrate = Q_concentrate;
    m_last_calculation.recovery = RECOVERY;
    m_last_calculation.rejection = REJECTION;
    m_last_calculation.pressure_pa = body->get_si_pressure();
    m_last_calculation.input_composition = std::move(input_composition);

    return {Q_permeate, Q_concentrate};
}

std::vector<SReportEntry> CShadowMembraneOsmos::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
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
        snprintf(buf, sizeof(buf), "%.3f", v);
        return Tstring(buf);
    };

    SReportEntry heading;
    heading.kind = EReportEntryKind::SectionHeading;
    heading.text = section_number + " Заявка на приобретение установки обратного осмоса "
                 + m_component->schematicName();
    report.push_back(heading);

    SReportEntry note;
    note.kind = EReportEntryKind::Text;
    note.text = "Опросный лист для вендора — не результат подбора из БД (см. чат): "
                "точная конфигурация мембран требует специализированного ПО у поставщика.";
    report.push_back(note);

    auto add_spec = [&](const Tstring &name, float value, const Tstring &unit)
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Specification;
        e.parameter_name = name;
        e.result = value;
        e.unit = unit;
        report.push_back(e);
    };

    add_spec("Расход на входе", m_last_calculation.Q_in, "м3/ч");
    add_spec("Давление на входе", m_last_calculation.pressure_pa, "Па");
    add_spec("Ожидаемое Recovery (TODO: оценка)", m_last_calculation.recovery * 100.0f, "%");
    add_spec("Ожидаемое Rejection (TODO: оценка)", m_last_calculation.rejection * 100.0f, "%");
    add_spec("Расход пермеата (оценка)", m_last_calculation.Q_permeate, "м3/ч");
    add_spec("Расход концентрата (оценка)", m_last_calculation.Q_concentrate, "м3/ч");

    for (const auto &[name, value, unit] : m_last_calculation.input_composition)
    {
        add_spec(name, value, unit);
    }

    return report;
}
