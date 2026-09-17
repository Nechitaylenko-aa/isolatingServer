#include "../include/CSimAccountNode.h"
#include "NComponent.h"
#include "CAccountNode.h"

namespace NCore
{
    CSimAccountNode::CSimAccountNode(NComponent *component)
        : ISimulationShadow(component)
    {}

    void CSimAccountNode::reset_runtime_state()
    {
        m_last_rate = 0.f;
    }

    void CSimAccountNode::tick(float /*dt*/)
    {
        auto node = dynamic_cast<CAccountNode*>(m_component);

        auto ob = node->ob();

        m_last_rate = ob ? ob->get_si_volume() : 1.0f;

        m_component->set_state_signal(
                SSignalRole{ SR_FLOW_RATE, 0, {}, {}, {} }, 0, m_last_rate);
    }

    void CSimAccountNode::apply_command(const SCommandRole &, bool) {} // узел учёта команд не принимает

    float CSimAccountNode::read(const SSignalRole &role, uint16_t) const
    {
        if (role.role == SR_FLOW_RATE) return m_last_rate;
        return 0.f;
    }
}
