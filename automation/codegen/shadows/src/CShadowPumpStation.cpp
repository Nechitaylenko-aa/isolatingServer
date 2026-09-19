
#include "../include/CShadowPumpStation.h"
#include "../../../IInstallationTemplate.h"
#include "../../../../components/CGenericComponent.h"
#include "CPumpStation.h"
#include "../../../CTechnologyBuilder.h"

namespace NCore
{
    CShadowPumpStation::CShadowPumpStation(NComponent *component) : ICodegenShadow(component) {}

    std::vector<SAutomationAtom> CShadowPumpStation::build_internal_topology(std::vector<SBoundaryInterlockCandidate> &candidates)
    {
        SRotationGroupSpec group_spec = build_rotation_spec();

        return build_rotation_atoms(group_spec, candidates);
    }

    ::std::vector<SBoundaryInterlockCandidate>
    CShadowPumpStation::internal_reactions(const SCollectedAutomationData &local_data)
    {
        // Авария->резерв — НЕ traversal здесь: станция и так владеет насосами напрямую
        // (см. решённый ранее вывод "внутри станции find_components_*_with_command
        // не нужна вообще — прямая итерация по своим потомкам"). Возвращаем пусто —
        // wire_boundary_patterns_impl не требуется для этого случая; оставлено на тот
        // редкий сценарий, если внутри сложного компонента появится реальная топология
        // с трубами МЕЖДУ разными типами внутренних компонентов (не параллельные ветки).
        return {};
    }

    uint16_t CShadowPumpStation::select_pump_to_start() const
    {
        uint16_t best = UINT16_MAX;

        if (m_rotation == RA_BY_HOURS)
        {
            float min_hours = std::numeric_limits<float>::max();
            for (uint16_t i = 0; i < m_pump_cells.size(); ++i)
            {
                if (m_pump_roles[i] != PR_WORKING || m_status[i] == 2) continue;
                if (m_moto_hours[i] < min_hours) { min_hours = m_moto_hours[i]; best = i; }
            }
        }
        else // RA_BY_LAST_STOP
        {
            uint16_t n = m_pump_cells.size();
            for (uint16_t k = 1; k <= n; ++k)
            {
                uint16_t i = (m_active_unit == UINT16_MAX ? 0 : m_active_unit + k) % n;
                if (m_pump_roles[i] == PR_WORKING && m_status[i] != 2) { best = i; break; }
            }
        }

        if (best == UINT16_MAX) // все рабочие в аварии — продвигаем резерв (п.3: автоматически)
            for (uint16_t i = 0; i < m_pump_cells.size(); ++i)
                if (m_pump_roles[i] == PR_STANDBY && m_status[i] != 2) { best = i; break; }

        return best;
    }

    CPumpStation *CShadowPumpStation::station() const
    {
        return dynamic_cast<CPumpStation*>(m_component);
    }

    void CShadowPumpStation::set_rotation(ERotationAlgorithm algorithm)
    {
        m_rotation = algorithm;
    }

    SRotationGroupSpec CShadowPumpStation::build_rotation_spec() const
    {
        SRotationGroupSpec spec;
        auto *self = dynamic_cast<CPumpStation*>(m_component);
        spec.unit_count    = self->pump_count();
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

    std::vector<SAutomationAtom> CShadowPumpStation::build_rotation_atoms(
            const SRotationGroupSpec &spec,
            const std::vector<SBoundaryInterlockCandidate> &external_candidates) const
    {
        std::vector<SAutomationAtom> result;
        auto *self = dynamic_cast<CPumpStation*>(m_component); // атом хранит NComponent* — самая обычная const-гимнастика для self-ссылки

        for (auto &c : external_candidates)
        {
            if (c.target_owner != self || c.reaction_role.role != spec.enable_role.role)
                continue;

            bool is_timed = (c.trigger.kind == TRIG_TIME);

            if (c.activate)
            {
                // СТАРТ: только среди рабочих (резерв — только через фолт-промоушн внутри Shadow)
                uint16_t start_count = is_timed ? 1 : spec.working_count;
                for (uint16_t i = 0; i < start_count; ++i)
                {
                    SAutomationAtom atom;
                    atom.trigger = c.trigger;   // как есть — TRIG_EVENT (боундари) или TRIG_TIME (chередование)

                    if (atom.trigger.kind == TRIG_EVENT)
                    for (uint16_t j = 0; j < spec.working_count; ++j)
                    {
                        if (j == i) continue;
                        SCondition hrs;
                        hrs.signal_owner = self;
                        hrs.role = spec.hours_role.role;
                        hrs.index = 0;//i;
                        hrs.op = (j < i) ? CMP_LT : CMP_LE; // детерминированный tie-break — младший индекс выигрывает
                        hrs.threshold_source = ESS_SIGNAL;
                        hrs.compare_owner = self;
                        hrs.compare_role = spec.hours_role.role;
                        hrs.compare_index = j;
                        atom.trigger.conditions.push_back(hrs);
                    }

                    SCommandRole cmd = spec.enable_role; cmd.index = 0;//i;

                    atom.actions.push_back({ self, cmd,  true });
                    atom.origin = AO_INTERNAL;
                    atom.scope = c.scope;
                    result.push_back(atom);
                }
            }
            else
            {
                // СТОП: по всем unit_count, включая промотированный резерв
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
                        running.index = 0;//i;
                        running.op = CMP_EQ;
                        running.threshold_source = ESS_FIXED;
                        running.fixed_threshold = 1.0f;
                        atom.trigger.conditions.push_back(running);
                    }

                    SCommandRole cmd = spec.enable_role; cmd.index = 0;//i;
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
