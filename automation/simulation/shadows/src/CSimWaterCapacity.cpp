#include "../include/CSimWaterCapacity.h"
//#include "../include/CSimPumpStation.h"
#include "NComponent.h"
//#include "../../../../../server/sources/logger-common/logger.h"
//#include "../../automation/CAutomationAtoms.h"
//#include <iostream>

namespace NCore
{
    CSimWaterCapacity::CSimWaterCapacity(NComponent *component) : ISimulationShadow(component)
    {
        // читать реальный паспортный объём ёмкости с компонента (m_equipProxy/CParameter),
        m_volume = 100.0f;
    }

    void CSimWaterCapacity::reset_runtime_state()
    {
        m_level = 0.5f; // старт с полу-заполненной, стоит решить осознанно позже
        m_inflow_pump = m_outflow_pump = nullptr;
    }

    void CSimWaterCapacity::setPumpBefore(ISimulationShadow *pump)
    {
        m_inflow_pump = pump;
    }
    void CSimWaterCapacity::setPumpPost(ISimulationShadow *pump)
    {
        m_outflow_pump = pump;
    }

    void CSimWaterCapacity::tick(float dt)
    {
        float rate = 0.f; // м3/с
        if (m_inflow_pump)  rate += 0.001;//m_inflow_pump->rated_flow_rate();
        if (m_outflow_pump) rate -= 0.001;//m_outflow_pump->rated_flow_rate();

        m_level += rate;//(rate * dt) / m_volume;
        m_level = std::clamp(m_level, 0.f, 1.f);

        bool hh    = m_level >= m_hh;
        bool upper = m_level >= m_upper && m_level < m_hh;
        bool mid   = m_level >= m_mid   && m_level < m_upper;
        bool bott  = m_level >= m_ll && m_level < m_mid;//m_level >= m_bott  && m_level < m_mid;
        bool ll    = m_level < m_ll;

        //  Компонент как почтовый ящик, далее эта инфа читается инструментом
        m_component->set_state_signal(SSignalRole{SR_LEVEL_HH,    0,{},{},{}}, 0, hh    ? 1.f : 0.f);
        m_component->set_state_signal(SSignalRole{SR_LEVEL_UPPER, 0,{},{},{}}, 0, upper ? 1.f : 0.f);
        m_component->set_state_signal(SSignalRole{SR_LEVEL_MID,   0,{},{},{}}, 0, mid   ? 1.f : 0.f);
        m_component->set_state_signal(SSignalRole{SR_LEVEL_BOTT,  0,{},{},{}}, 0, bott  ? 1.f : 0.f);
        m_component->set_state_signal(SSignalRole{SR_LEVEL_LL,    0,{},{},{}}, 0, ll    ? 1.f : 0.f);

    }


    void CSimWaterCapacity::apply_command(const SCommandRole &, bool) {}

    float CSimWaterCapacity::read(const SSignalRole &role, uint16_t) const
    {
        switch (role.role)
        {
            case SR_LEVEL_HH:    return m_level >= m_hh    ? 1.f : 0.f;
            case SR_LEVEL_UPPER: return m_level >= m_upper ? 1.f : 0.f;
            case SR_LEVEL_MID:   return m_level >= m_mid   ? 1.f : 0.f;
            case SR_LEVEL_BOTT:  return m_level <= m_bott  ? 1.f : 0.f;
            case SR_LEVEL_LL:    return m_level <= m_ll    ? 1.f : 0.f;
            default: return 0.f;
        }
    }
}
