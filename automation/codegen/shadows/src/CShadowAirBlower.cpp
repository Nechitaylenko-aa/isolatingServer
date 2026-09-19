
#include "../include/CShadowAirBlower.h"
#include "../../../IInstallationTemplate.h"
#include "../../../../components/CGenericComponent.h"
#include "CAirBlower.h"
#include "../../../CTechnologyBuilder.h"

namespace NCore
{
    CShadowAirBlower::CShadowAirBlower(NComponent *component) : ICodegenShadow(component) {}

    std::vector<SAutomationAtom> CShadowAirBlower::build_internal_topology(
            std::vector<SBoundaryInterlockCandidate> &candidates)
    {
        SRotationGroupSpec group_spec = build_rotation_spec();
        return build_rotation_atoms(group_spec, candidates);
    }

    std::vector<SBoundaryInterlockCandidate> CShadowAirBlower::internal_reactions(
            const SCollectedAutomationData &local_data)
    {
        // станция владеет воздуходувками напрямую,
        // traversal между внутренними компонентами не требуется.
        return {};
    }

    CAirBlower *CShadowAirBlower::station() const
    {
        return dynamic_cast<CAirBlower*>(m_component);
    }

    SRotationGroupSpec CShadowAirBlower::build_rotation_spec() const
    {
        SRotationGroupSpec spec;
        auto *self = dynamic_cast<CAirBlower*>(m_component);
        spec.unit_count    = self->blower_count();
        spec.working_count = (spec.unit_count == 3) ? 2 : spec.unit_count;
        spec.algorithm       = RA_BY_HOURS;
        spec.enable_role      = self->required_commands().front();
        spec.fault_reset_role = SCommandRole{ CR_PUMP_FAULT_RESET, 0, EMU_AMOUNT, EMUAMO::EA_ENUM,
                                              CS_COMPONENT_SCOPED, false, 0 };
        spec.hours_role  = SSignalRole{ SR_MOTO_HOURS, 0, E_MEASURE_UNITS::EMU_TIME, {},
                                        EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 };
        spec.status_role = SSignalRole{ SR_PUMP_STATUS, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                        EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 };
        return spec;
    }

    std::vector<SAutomationAtom> CShadowAirBlower::build_rotation_atoms(
            const SRotationGroupSpec &spec,
            const std::vector<SBoundaryInterlockCandidate> &external_candidates) const
    {

        std::vector<SAutomationAtom> result;
        auto *self = dynamic_cast<CAirBlower*>(m_component);

        for (auto &c : external_candidates)
        {
            if (c.target_owner != self || c.reaction_role.role != spec.enable_role.role)
                continue;

            bool is_timed = (c.trigger.kind == TRIG_TIME);

            if (c.activate)
            {
                uint16_t start_count = is_timed ? 1 : spec.working_count;
                for (uint16_t i = 0; i < start_count; ++i)
                {
                    SAutomationAtom atom;
                    atom.trigger = c.trigger;

                    if (atom.trigger.kind == TRIG_EVENT)
                    for (uint16_t j = 0; j < spec.working_count; ++j)
                    {
                        if (j == i) continue;
                        SCondition hrs;
                        hrs.signal_owner = self;
                        hrs.role = spec.hours_role.role;
                        hrs.index = i;
                        hrs.op = (j < i) ? CMP_LT : CMP_LE;
                        hrs.threshold_source = ESS_SIGNAL;
                        hrs.compare_owner = self;
                        hrs.compare_role = spec.hours_role.role;
                        hrs.compare_index = j;
                        atom.trigger.conditions.push_back(hrs);
                    }

                    SCommandRole cmd = spec.enable_role; cmd.index = i;
                    atom.actions.push_back({ self, cmd, true });
                    atom.origin = AO_INTERNAL;
                    atom.scope = c.scope;
                    result.push_back(atom);
                }
            }
            else
            {
                uint16_t stop_count = is_timed ? 1 : spec.unit_count;
                for (uint16_t i = 0; i < stop_count; ++i)
                {
                    SAutomationAtom atom;
                    atom.trigger = c.trigger;

                    if (atom.trigger.kind == TRIG_EVENT)
                    {
                        SCondition running;
                        running.signal_owner = self;
                        running.role = spec.status_role.role;
                        running.index = i;
                        running.op = CMP_EQ;
                        running.threshold_source = ESS_FIXED;
                        running.fixed_threshold = 1.0f;
                        atom.trigger.conditions.push_back(running);
                    }

                    SCommandRole cmd = spec.enable_role; cmd.index = i;
                    atom.actions.push_back({ self, cmd, false });
                    atom.origin = AO_INTERNAL;
                    atom.scope = c.scope;
                    result.push_back(atom);
                }
            }
        }
        return result;
    }
}
