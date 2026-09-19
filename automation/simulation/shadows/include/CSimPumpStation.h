//
// Created by artem on 28.08.26.
//

#ifndef NYM_PROJECT_CSIMPUMPSTATION_H
#define NYM_PROJECT_CSIMPUMPSTATION_H

#include "ISimulationShadow.h"
#include "../../../../components/CGenericComponent.h"

namespace NCore
{
    class CPumpStation;

    class CSimPumpStation : public ISimulationShadow
    {
    public:
        explicit CSimPumpStation(NComponent *component);
        ~CSimPumpStation() override;

        void  reset_runtime_state() override;
        void  tick(float dt) override;
        void  apply_command(const SCommandRole &role, bool activate) override;
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;

        [[nodiscard]] std::vector<NCore::SSignalRole> readable_state_roles() const override;


    private:
        [[nodiscard]] uint16_t select_pump_to_start() const;
        [[nodiscard]] CPumpStation *station() const;

    private:
        enum class ERotationAlgorithm {
            RA_BY_HOURS, RA_BY_LAST_STOP
        };
        enum class EPumpRole{
            PR_WORKING, PR_STANDBY
        };

        NComponent * m_owner;

        std::vector<CGenericComponent *> m_unit_cells;   // NComponent* по факту, но насос — CGenericComponent
        std::vector<EPumpRole> m_unit_roles;   // index-aligned
        std::vector<uint8_t> m_status;       // 0=IDLE,1=RUNNING,2=FAULT — index-aligned
        std::vector<float> m_moto_hours;   // index-aligned
        uint16_t m_active_unit{UINT16_MAX};
        ERotationAlgorithm m_rotation{ERotationAlgorithm::RA_BY_HOURS};
    };

}


#endif //NYM_PROJECT_CSIMPUMPSTATION_H
