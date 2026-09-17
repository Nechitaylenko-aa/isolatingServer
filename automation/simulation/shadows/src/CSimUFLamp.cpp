#include "../include/CSimUFLamp.h"

namespace NCore
{
    CSimUFLamp::CSimUFLamp(NComponent *component)
        : ISimulationShadow(component)
    {
    }

    void CSimUFLamp::reset_runtime_state()
    {
        m_status = 0;
    }

    void CSimUFLamp::tick(float /*dt*/)
    {
        // намеренно пусто
    }

    void CSimUFLamp::apply_command(const SCommandRole &role, bool activate)
    {
        if (role.role != CR_LAMP_ON_OFF)
            return;
        m_status = activate ? 1 : 0;
    }

    float CSimUFLamp::read(const SSignalRole &role, uint16_t /*index*/) const
    {
        if (role.role == SR_LAMP_STATUS)
            return (float)m_status;
        return 0.f;
    }
}
