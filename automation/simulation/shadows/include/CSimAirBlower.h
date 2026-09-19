
#ifndef NYM_PROJECT_CSIMAIRBLOWER_H
#define NYM_PROJECT_CSIMAIRBLOWER_H

#include "ISimulationShadow.h"
#include "../../../../components/CGenericComponent.h"

namespace NCore
{
    class CAirBlower;

    class CSimAirBlower : public ISimulationShadow
    {
    public:
        explicit CSimAirBlower(NComponent *component);
        ~CSimAirBlower() override;

        void  reset_runtime_state() override;
        void  tick(float dt) override;
        void  apply_command(const SCommandRole &role, bool activate) override;
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;
        [[nodiscard]] std::vector<SSignalRole> readable_state_roles() const override;

    private:
        [[nodiscard]] uint16_t select_unit_to_start() const;
        [[nodiscard]] CAirBlower *station() const;

    private:
        enum class ERotationAlgorithm { RA_BY_HOURS, RA_BY_LAST_STOP };
        enum class EUnitRole { PR_WORKING, PR_STANDBY };

        std::vector<CGenericComponent *> m_unit_cells;
        std::vector<EUnitRole> m_unit_roles;
        std::vector<uint8_t> m_status;
        std::vector<float> m_moto_hours;
        uint16_t m_active_unit{UINT16_MAX};
        ERotationAlgorithm m_rotation{ERotationAlgorithm::RA_BY_HOURS};
    };
}

#endif //NYM_PROJECT_CSIMAIRBLOWER_H
