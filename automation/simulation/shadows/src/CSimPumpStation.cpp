//
// Created by artem on 28.08.26.
//

#include "../include/CSimPumpStation.h"
#include "CPumpStation.h"
#include "../../../../../server/sources/logger-common/logger.h"
#include "../../../../components/water/include/CPumpStation.h"

NCore::CSimPumpStation::CSimPumpStation(NCore::NComponent *component) : ISimulationShadow(component)
{

    m_active_unit = 0;

    auto owner = dynamic_cast<CPumpStation*>(component);
    m_rotation = (ERotationAlgorithm)owner->rotation_algorithm();

    m_status.resize(owner->pump_count());
    m_moto_hours.resize(owner->pump_count());
    m_unit_roles.resize(owner->pump_count());


    m_unit_cells.clear();
    for (auto &comp : *(m_component->get_components()))
    {

        auto *pump = dynamic_cast<CGenericComponent*>(comp);
        if (pump)
            m_unit_cells.push_back(pump);
    }

    if (m_unit_cells.empty())
    {
        CLogger::instance().error("NCore::CSimPumpStation::reset_runtime_state",
                                  "No pumps inside CPumpStation");
    }


    CSimPumpStation::reset_runtime_state();
}

NCore::CSimPumpStation::~CSimPumpStation()
= default;

void NCore::CSimPumpStation::reset_runtime_state()
{
    // Обнуляем всё рабочее состояние, но не трогаем конфигурацию (роли насосов)
    for (size_t i = 0; i < m_status.size(); ++i)
    {
        m_status[i] = 0;
        m_moto_hours[i] = 0.0f;
    }

    m_active_unit = UINT16_MAX;

    m_rotation = ERotationAlgorithm::RA_BY_HOURS;

    m_unit_roles.clear();
    m_unit_roles.resize(m_unit_cells.size(), EPumpRole::PR_STANDBY);
    if (!m_unit_roles.empty())
    {
        m_unit_roles[0] = EPumpRole::PR_WORKING;
    }
}

void NCore::CSimPumpStation::tick(float dt)
{
    if (m_active_unit != UINT16_MAX && m_status[m_active_unit] == 1)
        m_moto_hours[m_active_unit] += dt / 3600.f;
}

void NCore::CSimPumpStation::apply_command(const NCore::SCommandRole &role, bool activate)
{
    if (role.role == CR_PUMP_START_STOP)
    {
        if (activate)
        {
            uint16_t idx = select_pump_to_start(); // решается только на старте (п.2)
            if (idx == UINT16_MAX)
            {
                m_component->set_error(EComponentError::ECE_CRITICAL);
                m_component->set_warnMessage("no available pump to start");
                return;
            }
            m_status[idx] = 1;
            m_active_unit = idx;
        }
        else if (m_active_unit != UINT16_MAX)
        {
            m_status[m_active_unit] = 0;
            //m_active_unit = UINT16_MAX;
        }
    }
    else if (role.role == CR_PUMP_FAULT_RESET)
    {
        for (unsigned char & m_statu : m_status)
        {
            if (m_statu == 2) m_statu = 0;
        }

        // п.4: промотированный резерв возвращается В РЕЗЕРВ, не вытесняется немедленно
        for (uint32_t i = 0; i < m_unit_roles.size(); ++i)
        {
            if (m_unit_roles[i] == EPumpRole::PR_STANDBY && m_status[i] == 1)
            {
                m_status[i] = 0; break;
            }
        }
    }
}

float NCore::CSimPumpStation::read(const NCore::SSignalRole &role, uint16_t index) const
{
    if (role.role == SR_PUMP_STATION_FAIL)
    {
        return std::all_of(m_status.begin(), m_status.end(), [](uint8_t s){ return s == 2; }) ? 1.f : 0.f;
    }
    if (role.role == SR_PUMP_STATUS)
    {
        if (m_active_unit != UINT16_MAX)
            return (float)m_status[m_active_unit];
        return (float)0;
    }

    return 0.f;
}

uint16_t NCore::CSimPumpStation::select_pump_to_start() const
{
    uint16_t best = UINT16_MAX;

    if (m_rotation == ERotationAlgorithm::RA_BY_HOURS)
    {
        float min_hours = std::numeric_limits<float>::max();
        for (uint16_t i = 0; i < m_unit_cells.size(); ++i)
        {
            if (m_unit_roles[i] != EPumpRole::PR_WORKING || m_status[i] == 2) continue;
            if (m_moto_hours[i] < min_hours) { min_hours = m_moto_hours[i]; best = i; }
        }
    }
    else // RA_BY_LAST_STOP
    {
        uint16_t n = m_unit_cells.size();
        for (uint16_t k = 1; k <= n; ++k)
        {
            uint16_t i = (m_active_unit == UINT16_MAX ? 0 : m_active_unit + k) % n;
            if (m_unit_roles[i] == EPumpRole::PR_WORKING && m_status[i] != 2) { best = i; break; }
        }
    }

    if (best == UINT16_MAX) // все рабочие в аварии — продвигаем резерв (п.3: автоматически)
        for (uint16_t i = 0; i < m_unit_cells.size(); ++i)
            if (m_unit_roles[i] == EPumpRole::PR_STANDBY && m_status[i] != 2) { best = i; break; }

    return best;
}

NCore::CPumpStation *NCore::CSimPumpStation::station() const
{
    return dynamic_cast<CPumpStation*>(m_component);
}

std::vector<NCore::SSignalRole> NCore::CSimPumpStation::readable_state_roles() const
{
    std::vector<SSignalRole> roles;
    roles.push_back(SSignalRole{ SR_PUMP_STATION_FAIL, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                 EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
    roles.push_back(SSignalRole{ SR_PUMP_STATUS, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                 EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
    return roles;
}
