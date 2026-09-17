#ifndef NYM_PROJECT_CSIMVALVECUT_H
#define NYM_PROJECT_CSIMVALVECUT_H

#include "ISimulationShadow.h"

namespace NCore
{
    class CSimValveCut : public ISimulationShadow
    {
    public:
        explicit CSimValveCut(NComponent *component);

        void  reset_runtime_state() override;
        void  tick(float) override {}
        void  apply_command(const SCommandRole &role, bool activate) override;
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;

        [[nodiscard]] bool is_open() const { return m_open; }
        void set_topology_changed_callback(std::function<void()> act) override;
        [[nodiscard]] std::vector<NCore::SSignalRole> readable_state_roles() const override;

    private:
        bool m_default_open{false};
        bool m_open{true};
        std::function<void()> m_cbTopologyChanged;
    };
}

#endif
