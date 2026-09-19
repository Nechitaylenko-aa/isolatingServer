//
// Создано по образцу CShadowLightFiletr.cpp
//

#include "../include/CShadowWaterCapacity.h"
#include "CParameter.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"
#include "../../IShadowManager.h"
#include "../../../../automation/IInstallationTemplate.h"
#include "../include/CShadowLightFiletr.h"
#include "../include/CShadowFilterIonEx.h"

namespace
{
    constexpr float WATER_DENSITY_KG_M3 = 1000.f;
    constexpr float G_ACCEL = 9.81f;
}

CShadowWaterCapacity::CShadowWaterCapacity(NCore::NComponent *component) : IShadow(component) {}

std::optional<float> CShadowWaterCapacity::sum_upstream_backwash_demand() const
{
    auto predicate = [](NCore::NComponent *c) -> bool
    {
        auto type = c->get_subtype();
        auto *water_type = std::get_if<NCore::E_WATER_COMPONENTS>(&type);
        if (!water_type)
        {
            return false;
        }
        return *water_type == NCore::EWB_WATER_FILTER_LIGHT
            || *water_type == NCore::EWB_WATER_FILTER_ION_EXCHANGE;
    };

    std::vector<NCore::CCell*> visited;
    auto neighbors = NCore::IInstallationTemplate::find_components_upstream_matching(
            m_component->input(0), predicate, visited);

    if (neighbors.empty())
    {
        return 0.f;  // фильтров выше по потоку нет вообще — не то же самое, что "не готовы"
    }

    float total = 0.f;
    for (auto *neighbor : neighbors)
    {
        auto *shadow = IShadowManager::getComponentShadow(neighbor);
        if (!shadow)
        {
            return std::nullopt;  // у соседа ещё нет тени вовсе — точно не готов
        }

        auto type = neighbor->get_subtype();
        auto *water_type = std::get_if<NCore::E_WATER_COMPONENTS>(&type);
        if (!water_type)
        {
            continue;
        }

        if (*water_type == NCore::EWB_WATER_FILTER_LIGHT)
        {
            auto v = static_cast<CShadowLightFilter*>(shadow)->backwash_volume_required();
            if (!v) return std::nullopt;
            total += *v;
        }
        else if (*water_type == NCore::EWB_WATER_FILTER_ION_EXCHANGE)
        {
            auto v = static_cast<CShadowFilterIonEx*>(shadow)->regeneration_rinse_volume_required();
            if (!v) return std::nullopt;
            total += *v;
        }
    }

    return total;
}

NCore::SEquipmentRequest
CShadowWaterCapacity::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    auto backwash_demand = sum_upstream_backwash_demand();
    if (!backwash_demand)
    {
        return {};  // выше по потоку есть фильтр, ещё не подобранный — ждём следующего цикла
    }

    // ВНИМАНИЕ, частично открытый вопрос: расход (m_Q) как критерий подбора остаётся,
    // но реального объёма "просто на всякий случай" (буфер часов работы) — не считаю,
    // это отдельно от объёма промывки и зависит от ТЗ станции (time_clean_water_reserve_hour
    // в CDrinkWaterTor, когда дойдём до неё) — см. чат.
    float m_Q = tor->hourInputMax()->si_value();  // м³/ч

    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;
    req.params = {
            m_Q,             // [0] м³/ч — расход
            *backwash_demand, // [1] м³ — суммарный объём на промывку фильтров выше по потоку
    };

    return req;
}

std::vector<float>
CShadowWaterCapacity::getCalculationsWithEquip(NCore::SEquipLight &equip_proxy, NCore::COperatingBody *body,
                                                IGeneralTor *tor)
{
    std::vector<float> res;

    if (equip_proxy.params.size() < 3)
    {
        fprintf(stderr, "CShadowWaterCapacity: received less than 3 equipment fields\n");
        return res;
    }

    float width    = equip_proxy.params.at(0);
    float deepness = equip_proxy.params.at(1);
    float height   = equip_proxy.params.at(2);
    float volume   = width * deepness * height;

    res.push_back(width);
    res.push_back(deepness);
    res.push_back(height);
    res.push_back(volume);

    m_last_calculation.has_data = true;
    m_last_calculation.width = width;
    m_last_calculation.deepness = deepness;
    m_last_calculation.height = height;
    m_last_calculation.volume = volume;

    return res;
}

std::optional<float> CShadowWaterCapacity::min_required_pressure_pa() const
{
    if (!m_last_calculation.has_data)
    {
        return std::nullopt;
    }

    return WATER_DENSITY_KG_M3 * G_ACCEL * m_last_calculation.height;
}

std::vector<SReportEntry> CShadowWaterCapacity::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
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

    SReportEntry heading;
    heading.kind = EReportEntryKind::SectionHeading;
    heading.text = section_number + " Подбор ёмкости " + m_component->schematicName();
    report.push_back(heading);

    SReportEntry e;
    e.kind = EReportEntryKind::Formula;
    e.parameter_name = "Объём ёмкости";
    e.formula_symbolic = "V = width * deepness * height";
    e.substituted = "V = " + fmt(m_last_calculation.width) + " * " + fmt(m_last_calculation.deepness)
                   + " * " + fmt(m_last_calculation.height);
    e.result = m_last_calculation.volume;
    e.unit = "м3";
    e.formula_number = formula_start;
    report.push_back(e);

    return report;
}
