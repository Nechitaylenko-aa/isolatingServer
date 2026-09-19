//
// Создано по образцу CShadowWaterCapacity.cpp
//

#include "../include/CShadowPumpStation.h"
#include "CParameter.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include <NComponent.h>
#include "../../IShadowManager.h"
#include "../../../../automation/IInstallationTemplate.h"
#include "../include/CShadowWaterCapacity.h"
#include "../include/CShadowLightFiletr.h"
#include <limits>

CShadowPumpStation::CShadowPumpStation(NCore::NComponent *component) : IShadow(component) {}

std::optional<CShadowPumpStation::SDownstreamDemand> CShadowPumpStation::gather_downstream_demand() const
{
    auto predicate = [](NCore::NComponent *c) -> bool
    {
        auto type = c->get_subtype();
        auto *water_type = std::get_if<NCore::E_WATER_COMPONENTS>(&type);
        if (!water_type)
        {
            return false;
        }
        return *water_type == NCore::EWB_CAPACITY
               || *water_type == NCore::EWB_WATER_FILTER_LIGHT;
    };

    std::vector<NCore::CCell*> visited;
    auto neighbors = NCore::IInstallationTemplate::find_components_downstream_matching(
            m_component->output(0), predicate, visited);

    SDownstreamDemand demand;

    for (auto *neighbor : neighbors)
    {
        auto *shadow = IShadowManager::getComponentShadow(neighbor);
        if (!shadow)
        {
            return std::nullopt;  // сосед найден, тени ещё нет — не готов
        }

        auto type = neighbor->get_subtype();
        auto *water_type = std::get_if<NCore::E_WATER_COMPONENTS>(&type);
        if (!water_type)
        {
            continue;
        }

        if (*water_type == NCore::EWB_CAPACITY)
        {
            auto v = static_cast<CShadowWaterCapacity*>(shadow)->min_required_pressure_pa();
            if (!v) return std::nullopt;
            demand.min_required = std::max(demand.min_required, *v);  // самая высокая ёмкость решает
        }
        else if (*water_type == NCore::EWB_WATER_FILTER_LIGHT)
        {
            auto v = static_cast<CShadowLightFilter*>(shadow)->max_allowed_pressure_pa();
            if (!v) return std::nullopt;
            if (!demand.has_upper_bound || *v < demand.max_allowed)
            {
                demand.max_allowed = *v;      // самый строгий фильтр решает
                demand.has_upper_bound = true;
            }
        }
    }

    return demand;
}

NCore::SEquipmentRequest
CShadowPumpStation::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    auto demand = gather_downstream_demand();
    if (!demand)
    {
        return {};  // сосед ещё не подобран — ждём следующего цикла
    }

    // ВНИМАНИЕ, открытый вопрос: если ниже по потоку нет ни ёмкости, ни фильтра вообще (насос,
    // например, последний перед потребителем) — сейчас просто нечего запросить, возвращаю {}.
    // По-хорошему тут должен подключаться water_out_press() из ТЗ станции (CDrinkWaterTor), но
    // это отдельный случай, не решаю его здесь заодно.
    if (demand->min_required <= 0.f && !demand->has_upper_bound)
    {
        return {};
    }

    if (demand->has_upper_bound && demand->min_required > demand->max_allowed)
    {
        // Конфликт проекта: то, что нужно ёмкости, продавит фильтр. Не выбираю тихо одну из
        // сторон — сигнализирую и не отправляю запрос вообще, пусть это увидят на этапе проектирования.
        m_component->set_error(EComponentError::ECE_CRITICAL);
        m_component->set_warnMessage(
                "Pump head required to fill downstream tank exceeds the pressure limit of a "
                "downstream filter — conflicting requirements, redesign needed (e.g. pressure "
                "reducing valve between pump and filter).");
        return {};
    }

    float m_Q = tor->hourInputMax()->si_value();  // м³/ч
    float max_allowed = demand->has_upper_bound ? demand->max_allowed : std::numeric_limits<float>::max();

    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;
    req.params = {
            m_Q,                 // [0] м³/ч — расход
            demand->min_required, // [1] Па — минимальный напор (0, если ёмкостей ниже нет)
            max_allowed,          // [2] Па — предел, который нельзя превышать (FLT_MAX, если фильтров ниже нет)
    };

    return req;
}

std::vector<float>
CShadowPumpStation::getCalculationsWithEquip(NCore::SEquipLight &equip_proxy, NCore::COperatingBody *body,
                                             IGeneralTor *tor)
{
    std::vector<float> res;

    if (equip_proxy.params.empty())
    {
        fprintf(stderr, "CShadowPumpStation: received 0 equipment fields\n");
        return res;
    }

    float selected_pressure = equip_proxy.params.at(0);  // Па — паспортное давление выбранного насоса
    res.push_back(selected_pressure);

    auto demand = gather_downstream_demand();

    m_last_calculation.has_data = true;
    m_last_calculation.selected_pressure = selected_pressure;
    m_last_calculation.min_required = demand ? demand->min_required : 0.f;
    m_last_calculation.max_allowed = (demand && demand->has_upper_bound) ? demand->max_allowed : 0.f;
    m_last_calculation.has_upper_bound = demand && demand->has_upper_bound;

    return res;
}

std::vector<SReportEntry> CShadowPumpStation::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
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
    heading.text = section_number + " Подбор насосной станции " + m_component->schematicName();
    report.push_back(heading);

    SReportEntry e;
    e.kind = EReportEntryKind::Formula;
    e.parameter_name = "Напор насосной станции";
    e.formula_symbolic = "P_min <= P_pump <= P_max";
    e.substituted = "P_min = " + fmt(m_last_calculation.min_required)
                    + (m_last_calculation.has_upper_bound ? (", P_max = " + fmt(m_last_calculation.max_allowed)) : Tstring(", P_max = не ограничено"));
    e.result = m_last_calculation.selected_pressure;
    e.unit = "Па";
    e.formula_number = formula_start;
    report.push_back(e);

    return report;
}