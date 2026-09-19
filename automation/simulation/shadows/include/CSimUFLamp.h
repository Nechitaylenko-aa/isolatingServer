//
// Симуляционная тень для CUFLamp — чистое хранилище статуса вкл/выкл, по образцу
// CSimLightFilter/CSimFilterSorption.
//

#ifndef NYM_PROJECT_CSIMUFLAMP_H
#define NYM_PROJECT_CSIMUFLAMP_H

#include "ISimulationShadow.h"

namespace NCore
{
    class CSimUFLamp : public ISimulationShadow
    {
    public:
        explicit CSimUFLamp(NComponent *component);
        ~CSimUFLamp() override = default;

        void  reset_runtime_state() override;
        void  tick(float dt) override; // no-op, чистое состояние без физики времени
        void  apply_command(const SCommandRole &role, bool activate) override;
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;

    private:
        uint8_t m_status{0}; // 0 = выкл, 1 = вкл
    };
}

#endif //NYM_PROJECT_CSIMUFLAMP_H
