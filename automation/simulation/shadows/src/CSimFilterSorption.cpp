//
// Симуляционная тень для CFilterSorption — см. комментарий в заголовке.
//

#include "../include/CSimFilterSorption.h"
#include "CFilterSorption.h"

namespace NCore
{
    CSimFilterSorption::CSimFilterSorption(NComponent *component)
        : ISimulationShadow(component)
    {
        CSimFilterSorption::reset_runtime_state();
    }

    void CSimFilterSorption::reset_runtime_state()
    {
        m_valve_status = { 0, 0 };
    }

    void CSimFilterSorption::tick(float /*dt*/)
    {
        // Намеренно пусто: конец цикла промывки — не функция времени, а реакция на
        // внешнее событие (например, уровень приёмной ёмкости), которая приходит как
        // обычная apply_command(CR_FLOW_DIRECTION_VALVE, false) через атомы/боундари-
        // паттерн. Тень не считает и не хранит длительность.
    }

    void CSimFilterSorption::apply_command(const SCommandRole &role, bool activate)
    {
        /*if (role.role != CR_FLOW_DIRECTION_VALVE)
            return;*/

        if (role.index >= m_valve_status.size())
            return;

        m_valve_status[role.index] = activate ? 1 : 0;
    }

    float CSimFilterSorption::read(const SSignalRole &role, uint16_t index) const
    {
        if (index >= m_valve_status.size())
            return 0.f;
        return (float)m_valve_status.at(index);
        /*if (role.role == SR_VALVE_STATUS && index < m_valve_status.size())
            return (float)m_valve_status[index];

        return 0.f;*/
    }

    CFilterSorption *CSimFilterSorption::filter() const
    {
        return dynamic_cast<CFilterSorption*>(m_component);
    }
}
