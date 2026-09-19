//
// Симуляционная тень для CLightFilter — см. комментарий в заголовке.
// По аналогии с CSimFilterSorption.
//

#include "../include/CSimLightFilter.h"
#include "CLightFilter.h"

namespace NCore
{
    CSimLightFilter::CSimLightFilter(NComponent *component)
        : ISimulationShadow(component)
    {
        CSimLightFilter::reset_runtime_state();
    }

    void CSimLightFilter::reset_runtime_state()
    {
        m_valve_status = { 0, 0 };
    }

    void CSimLightFilter::tick(float /*dt*/)
    {
        // Намеренно пусто: конец цикла промывки — не функция времени, а реакция на
        // внешнее событие, приходящее как обычная apply_command(CR_FLOW_DIRECTION_VALVE,
        // false) через атомы/боундари-паттерн. Тень не считает и не хранит длительность.
    }

    void CSimLightFilter::apply_command(const SCommandRole &role, bool activate)
    {
        if (role.role != CR_FLOW_DIRECTION_VALVE)
            return;

        if (role.index >= m_valve_status.size())
            return;

        m_valve_status[role.index] = activate ? 1 : 0;
    }

    float CSimLightFilter::read(const SSignalRole &role, uint16_t index) const
    {
        if (index >= m_valve_status.size())
            return 0.f;
        return (float)m_valve_status.at(index);
        /*if (role.role == SR_VALVE_STATUS && index < m_valve_status.size())
            return (float)m_valve_status[index];

        return 0.f;*/
    }

    CLightFilter *CSimLightFilter::filter() const
    {
        return dynamic_cast<CLightFilter*>(m_component);
    }
}
