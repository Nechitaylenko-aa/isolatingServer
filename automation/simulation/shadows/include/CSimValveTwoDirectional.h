#ifndef NYM_PROJECT_CSIMVALVETWODIRECTIONAL_H
#define NYM_PROJECT_CSIMVALVETWODIRECTIONAL_H

#include "ISimulationShadow.h"

namespace NCore
{
    class CSimValveTwoDirectional : public ISimulationShadow
    {
    public:
        explicit CSimValveTwoDirectional(NComponent *component);

        void  reset_runtime_state() override;
        void  tick(float) override {}
        void  apply_command(const SCommandRole &role, bool activate) override;
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;
        void set_topology_changed_callback(std::function<void()>) override;

        //!< для CSimGraphManager — какая ветка (index в output()) сейчас активна
        [[nodiscard]] uint16_t active_branch() const { return m_active_branch; }

    private:
        uint16_t m_default_branch{0};
        uint16_t m_alt_branch{1};
        uint16_t m_active_branch{0};
        std::function<void()> m_cbTopologyChanged; // подписка от CSimGraphManager, задаётся отдельно
    };
}

#endif
