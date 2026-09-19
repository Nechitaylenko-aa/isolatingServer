#include "../include/CSimValveCut.h"

namespace NCore
{
    CSimValveCut::CSimValveCut(NComponent *component) : ISimulationShadow(component) {}

    void CSimValveCut::reset_runtime_state() { m_open = m_default_open; }

    void CSimValveCut::apply_command(const SCommandRole &role, bool activate)
    {
        if (activate == m_open) return;
        m_open = activate;
        if (m_cbTopologyChanged) m_cbTopologyChanged();
    }

    float CSimValveCut::read(const SSignalRole &, uint16_t) const { return m_open ? 1.f : 0.f; }

    void CSimValveCut::set_topology_changed_callback(std::function<void()> act)
    {
        m_cbTopologyChanged = std::move(act);
    }

    std::vector<NCore::SSignalRole> CSimValveCut::readable_state_roles() const
    {
        std::vector<SSignalRole> roles; 

        roles.push_back(SSignalRole{ SR_VALVE_STATUS, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                     EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
        return roles;
    }
}
