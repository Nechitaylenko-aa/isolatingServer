//
// Created by artem on 24.08.26.
//

#ifndef NYM_PROJECT_CSHADOWPUMPSTATION_H
#define NYM_PROJECT_CSHADOWPUMPSTATION_H


#include "../../ICodegenShadow.h"



namespace NCore
{
    class CPumpStation;
    class CGenericComponent;

    enum EPumpRole
    {
        PR_WORKING, PR_STANDBY
    };

    enum ERotationAlgorithm : uint8_t { RA_BY_HOURS, RA_BY_LAST_STOP };

    struct SRotationGroupSpec
    {
        uint16_t            unit_count{0};
        uint16_t            working_count{0};   // НОВОЕ: [0, working_count) — обычная ротация;
        // [working_count, unit_count) — резерв, включается
        // только фолт-промоушном внутри Shadow, не статическими атомами
        ERotationAlgorithm  algorithm{RA_BY_HOURS};
        SCommandRole        enable_role{};
        SCommandRole        fault_reset_role{};
        SSignalRole         hours_role{};
        SSignalRole         status_role{};
    };

    class CShadowPumpStation : public ICodegenShadow
    {
    public:
        explicit CShadowPumpStation(NComponent *component);

        std::vector<SAutomationAtom> build_internal_topology(std::vector<SBoundaryInterlockCandidate> &candidates) override;

        std::vector<SBoundaryInterlockCandidate>
        internal_reactions(const SCollectedAutomationData &local_data) override;

        void  set_rotation(ERotationAlgorithm algorithm);

        [[nodiscard]] SRotationGroupSpec build_rotation_spec() const;
        [[nodiscard]] std::vector<SAutomationAtom> build_rotation_atoms(
                const SRotationGroupSpec &spec,
                const std::vector<SBoundaryInterlockCandidate> &external_candidates) const;

    private:
        [[nodiscard]] uint16_t select_pump_to_start() const;

        [[nodiscard]] CPumpStation *station() const; // todo: return static_cast<CPumpStation*>(m_component);

        std::vector<CGenericComponent *> m_pump_cells;   // NComponent* по факту, но насос — CGenericComponent
        std::vector<EPumpRole> m_pump_roles;   // index-aligned
        std::vector<uint8_t> m_status;       // 0=IDLE,1=RUNNING,2=FAULT — index-aligned
        std::vector<float> m_moto_hours;   // index-aligned
        uint16_t m_active_unit{UINT16_MAX};
        ERotationAlgorithm m_rotation{RA_BY_HOURS};
    };
}

#endif //NYM_PROJECT_CSHADOWPUMPSTATION_H
