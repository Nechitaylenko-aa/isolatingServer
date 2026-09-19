#ifndef NYM_PROJECT_CSIMWATERCAPACITY_H
#define NYM_PROJECT_CSIMWATERCAPACITY_H

#include "ISimulationShadow.h"

namespace NCore
{
    class CSimPumpStation; // сосед, откуда/куда идёт расход

    class CSimWaterCapacity : public ISimulationShadow
    {
    public:
        explicit CSimWaterCapacity(NComponent *component);

        void  reset_runtime_state() override;
        void  tick(float dt) override;
        void  apply_command(const SCommandRole &role, bool activate) override; // no-op — ёмкость не принимает команд
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;

        //!< вызывается раннером из process_command при старте/останове соседнего насоса
        void  setPumpBefore(ISimulationShadow *pump); //!< наполняет (downstream сосед)
        void  setPumpPost(ISimulationShadow *pump);   //!< осушает (upstream сосед)

    private:
        float m_level{0};        //!< 0..1, доля от паспортного объёма
        float m_volume{0};       //!< м3 — паспортный объём, читается с компонента в конструкторе

        ISimulationShadow *m_inflow_pump{nullptr};
        ISimulationShadow *m_outflow_pump{nullptr};

        //!< пороги в долях 0..1 — читаются design-time с компонента/ToR при конструировании
        float m_hh{0.95f}, m_upper{0.8f}, m_mid{0.5f}, m_bott{0.2f}, m_ll{0.05f};
    };
}

#endif
