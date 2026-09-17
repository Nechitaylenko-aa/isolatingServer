//
// Created by artem on 25.08.26.
//

#ifndef NYM_PROJECT_ICODEGENSHADOW_H
#define NYM_PROJECT_ICODEGENSHADOW_H

#include "../CAutomationAtoms.h"


namespace NCore
{
    class IInstallationTemplate;
    struct SBoundaryInterlockCandidate;
    struct SCollectedAutomationData;

    class ICodegenShadow
    {
    public:
        virtual ~ICodegenShadow() = default;

        virtual std::vector<SAutomationAtom> build_internal_topology(std::vector<SBoundaryInterlockCandidate> &candidates) = 0;
        virtual std::vector<SBoundaryInterlockCandidate>
        internal_reactions(const SCollectedAutomationData &local_data) = 0;

    protected:
        explicit ICodegenShadow(NComponent *component) : m_component(component) {}
        NComponent *m_component;
    };

} // NCore

#endif //NYM_PROJECT_ICODEGENSHADOW_H
