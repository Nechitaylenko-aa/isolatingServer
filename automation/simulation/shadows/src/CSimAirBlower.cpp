//
// CSimAirBlower — структурная копия CSimPumpStation. Известный баг оригинала
// (m_status/m_moto_hours никогда не ресайзятся — см. обсуждение в чате про
// CSimPumpStation) здесь ИСПРАВЛЕН сразу, не скопирован.
//

#include "../include/CSimAirBlower.h"
#include "CAirBlower.h"
#include "../../../../../server/sources/logger-common/logger.h"

namespace NCore
{
    CSimAirBlower::CSimAirBlower(NComponent *component) : ISimulationShadow(component)
    {
        m_rotation = ERotationAlgorithm::RA_BY_LAST_STOP;
        m_active_unit = 0;

        auto owner = dynamic_cast<CAirBlower*>(component);

        m_status.resize(owner->get_components()->size());
        m_moto_hours.resize(owner->get_components()->size());
        m_unit_roles.resize(owner->get_components()->size());

        component->rebuild_internal_topology();

        m_unit_cells.clear();
        for (auto &comp : *(m_component->get_components()))
        {
            auto *unit = dynamic_cast<CGenericComponent*>(comp);
            if (unit)
                m_unit_cells.push_back(unit);
        }

        if (m_unit_cells.empty())
        {
            CLogger::instance().error("NCore::CSimAirBlower::CSimAirBlower",
                                      "No blower units inside CAirBlower");
        }
        m_status = {0, 0};
    }

    CSimAirBlower::~CSimAirBlower()
    = default;

    void CSimAirBlower::reset_runtime_state()
    {

        m_status.assign(m_unit_cells.size(), 0);
        m_moto_hours.assign(m_unit_cells.size(), 0.0f);

        m_active_unit = UINT16_MAX;
        m_rotation = ERotationAlgorithm::RA_BY_HOURS;

        m_unit_roles.clear();
        m_unit_roles.resize(m_unit_cells.size(), EUnitRole::PR_STANDBY);
        if (!m_unit_roles.empty())
            m_unit_roles[0] = EUnitRole::PR_WORKING;
    }

    void CSimAirBlower::tick(float dt)
    {
        if (m_active_unit != UINT16_MAX && m_active_unit < m_status.size() && m_status[m_active_unit] == 1)
            m_moto_hours[m_active_unit] += dt / 3600.f;

        // TODO: как и у насоса — фолт-продвижение резерва не реализовано (заглушка на будущее)
    }

    void CSimAirBlower::apply_command(const SCommandRole &role, bool activate)
    {
        /*if (role.role != CR_BLOWER_START_STOP)
            return;*/

        if (activate)
        {
            uint16_t idx = select_unit_to_start();
            if (idx == UINT16_MAX)
            {
                m_component->set_error(EComponentError::ECE_CRITICAL);
                m_component->set_warnMessage("no available blower unit to start");
                return;
            }
            m_status[idx] = 1;
            m_active_unit = idx;
        }
        else if (m_active_unit != UINT16_MAX)
        {
            m_status[m_active_unit] = 0;
            m_active_unit = UINT16_MAX;
        }
    }

    float CSimAirBlower::read(const SSignalRole &role, uint16_t index) const
    {

        if (role.role == SR_PUMP_STATION_FAIL)
            return std::all_of(m_status.begin(), m_status.end(), [](uint8_t s){ return s == 2; }) ? 1.f : 0.f;
        if (role.role == SR_PUMP_STATUS && index < m_status.size())
            if (m_active_unit != UINT16_MAX)
                return (float)m_status[m_active_unit];
        return 0.f;
    }

    std::vector<SSignalRole> CSimAirBlower::readable_state_roles() const
    {
        return {
                {SR_PUMP_STATUS},
                {SR_PUMP_STATION_RUNNING}
        };
        std::vector<SSignalRole> roles;
        /*roles.reserve(m_status.size());
        for (uint16_t i = 0; i < m_status.size(); ++i)
        {
            roles.push_back(SSignalRole{ SR_PUMP_STATUS, i, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                         EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
        }*/
        roles.push_back(SSignalRole{ SR_PUMP_STATION_FAIL, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                     EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
        roles.push_back(SSignalRole{ SR_PUMP_STATUS, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                     EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
        return roles;
    }

    uint16_t CSimAirBlower::select_unit_to_start() const
    {
        uint16_t best = UINT16_MAX;

        if (m_rotation == ERotationAlgorithm::RA_BY_HOURS)
        {
            float min_hours = std::numeric_limits<float>::max();
            for (uint16_t i = 0; i < m_unit_cells.size(); ++i)
            {
                if (m_unit_roles[i] != EUnitRole::PR_WORKING || m_status[i] == 2) continue;
                if (m_moto_hours[i] < min_hours) { min_hours = m_moto_hours[i]; best = i; }
            }
        }
        else
        {
            uint16_t n = m_unit_cells.size();
            for (uint16_t k = 1; k <= n; ++k)
            {
                uint16_t i = (m_active_unit == UINT16_MAX ? 0 : m_active_unit + k) % n;
                if (m_unit_roles[i] == EUnitRole::PR_WORKING && m_status[i] != 2) { best = i; break; }
            }
        }

        if (best == UINT16_MAX)
            for (uint16_t i = 0; i < m_unit_cells.size(); ++i)
                if (m_unit_roles[i] == EUnitRole::PR_STANDBY && m_status[i] != 2) { best = i; break; }

        return best;
    }

    CAirBlower *CSimAirBlower::station() const
    {
        return dynamic_cast<CAirBlower*>(m_component);
    }
}
