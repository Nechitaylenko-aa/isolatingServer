#include "../include/CSimValveTwoDirectional.h"

namespace NCore
{
    CSimValveTwoDirectional::CSimValveTwoDirectional(NComponent *component) : ISimulationShadow(component) {}

    void CSimValveTwoDirectional::reset_runtime_state() { m_active_branch = m_default_branch; }

    void CSimValveTwoDirectional::apply_command(const SCommandRole &role, bool activate)
    {
        /*if (role.role != CR_FLOW_DIRECTION_VALVE)
            return;*/
        uint16_t new_branch = activate ? m_alt_branch : m_default_branch;
        if (new_branch == m_active_branch) return;
        m_active_branch = new_branch;

        if (m_cbTopologyChanged)
            m_cbTopologyChanged();
    }

    float CSimValveTwoDirectional::read(const SSignalRole &, uint16_t) const { return (float)m_active_branch; }

    void CSimValveTwoDirectional::set_topology_changed_callback(std::function<void()> callback)
    {
        m_cbTopologyChanged = std::move(callback);
    }
}
