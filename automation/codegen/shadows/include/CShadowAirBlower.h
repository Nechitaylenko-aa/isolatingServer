
#ifndef NYM_PROJECT_CSHADOWAIRBLOWER_H
#define NYM_PROJECT_CSHADOWAIRBLOWER_H

#include "../../ICodegenShadow.h"
#include "CShadowPumpStation.h"


namespace NCore
{
    class CAirBlower;
    class CGenericComponent;

    class CShadowAirBlower : public ICodegenShadow
    {
    public:
        explicit CShadowAirBlower(NComponent *component);

        std::vector<SAutomationAtom> build_internal_topology(
                std::vector<SBoundaryInterlockCandidate> &candidates) override;

        std::vector<SBoundaryInterlockCandidate>
        internal_reactions(const SCollectedAutomationData &local_data) override;

        [[nodiscard]] SRotationGroupSpec build_rotation_spec() const;
        [[nodiscard]] std::vector<SAutomationAtom> build_rotation_atoms(
                const SRotationGroupSpec &spec,
                const std::vector<SBoundaryInterlockCandidate> &external_candidates) const;

    private:
        [[nodiscard]] CAirBlower *station() const;
    };
}

#endif //NYM_PROJECT_CSHADOWAIRBLOWER_H
