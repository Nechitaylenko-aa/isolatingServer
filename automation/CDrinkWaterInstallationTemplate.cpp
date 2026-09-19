#include "CDrinkWaterInstallationTemplate.h"
#include "../../saver/include/CSubProject.h"
#include "CAutomationModel.h"
#include "actuator/Instrument.h"
#include "CAutomationCollector.h"
#include "NComponent.h"
#include <iostream>
#include "codegen/CCodegenShadowsManager.h"
#include "../../server/sources/logger-common/logger.h"
#include <CDrinkWaterTor.h>
#include <set>

namespace NCore
{
    static CCap* find_signal_anchor(const SCollectedAutomationData &data,
                                    NComponent *owner, const SSignalRole &role)
    {
        for (auto &s : data.signals)
            if (s.owner == owner && s.role.role == role.role && s.role.index == role.index)
                return s.anchor_cap;
        return nullptr;
    }

    static CCap* find_command_anchor(const SCollectedAutomationData &data,
                                     NComponent *owner, const SCommandRole &role)
    {
        for (auto &c : data.commands)
        {
            if (c.owner == owner && c.role.role == role.role && c.role.index == role.index)
            {
                return c.anchor_cap;
            }
        }
        return nullptr;
    }

    bool CDrinkWaterInstallationTemplate::bind(CSubProject *proj, const SCollectedAutomationData &data)
    {
        m_subproject = proj;

        for (auto *item : *proj->project_cell()->get_components())
        {
            if(auto *comp = dynamic_cast<NComponent *>(item))
            {
                comp->rebuild_internal_topology();
            }
        }

        CLogger::instance().init("", ELogLevel::LL_DEBUG);

        // 1. заводим атомы инструменты и актауторы для безусловных процессов (HIGH_LEVEL -> STOP_BEFORE_PUMP)
        auto *model = proj->automation_model();

        std::vector<SBoundaryInterlockCandidate> candidates = wire_boundary_patterns(proj, data);

        // 2. Заводим набор атомов для "нормального" течения процесса
        uint16_t normal_id = add_atom_set(model, 0, "SET_NORMAL");
        set_initial_active_set(model, normal_id);

        std::set<NComponent*> internal_atoms_done;

        for (auto &c : candidates)
        {
            if (c.signal_owner->component_error() == EComponentError::ESE_ACTUATOR)
            {
                //CLogger::instance().shutdown();
                return false;
            }

            CCap *signal_anchor  = find_signal_anchor(data, c.signal_owner, c.signal_role);
            CCap *command_anchor = find_command_anchor(data, c.target_owner, c.reaction_role);

            if (!signal_anchor || !command_anchor)
            {
                assert(signal_anchor && command_anchor);
                continue;
            }

            if (is_state_derived_role(c.signal_role.role))
                assert(add_internal_instrument(model, c.signal_owner, c.signal_role));
            else
                assert(add_instrument(model, signal_anchor, c.signal_role));

            add_actuator(model, command_anchor, c.reaction_role);

            auto shadow = CCodegenShadowsManager::codegen_shadow_instance(c.target_owner);
            if (shadow)
            {
                if (internal_atoms_done.insert(c.target_owner).second) // true только один раз на компонент
                {
                    auto atoms = shadow->build_internal_topology(candidates);
                    if (!atoms.empty())
                    {
                        for (auto &atom: atoms)
                        {
                            if (atom.scope == BRS_SUPERVISORY)
                                add_supervisory_atom(model, atom);
                            else
                                add_atom_to_set(model, normal_id, atom);
                        }

                        continue;
                    }
                }
                else
                {
                    continue;
                }
            }

            SAutomationAtom atom;
            SCondition cond;

            cond.signal_owner = c.signal_owner;
            atom.trigger.kind = TRIG_EVENT;
            cond.role = c.signal_role.role;
            cond.index = c.signal_role.index;
            cond.op = CMP_EQ;
            cond.threshold_source = ESS_FIXED;
            cond.fixed_threshold = 1.0f;
            atom.trigger.conditions.push_back(cond);
            atom.actions.push_back(SAtomCommand{ c.target_owner, c.reaction_role, c.activate });
            atom.origin = AO_BOUNDARY;

            if (c.scope == BRS_SUPERVISORY)
                add_supervisory_atom(model, atom);
            else
                add_atom_to_set(model, normal_id, atom);
        }

        // all variables
        {
            for (auto *item: *proj->project_cell()->get_components())
            {
                auto *comp = dynamic_cast<NComponent*>(item);
                for (auto vb: comp->variable_behaviors())
                {
                    vb.variable.owner = comp;
                    add_internal_variable(model, vb);
                }
            }
        }

        std::vector<SDeclaredAutomation> undefined = wire_schedule_atoms(model, data, normal_id);
        std::vector<SBackwashCandidate> backwashAgents = wire_filter_backwash_pattern(proj, data);

        bool ok = true;
        for (auto &rec : backwashAgents)
        {
            if (!rec.is_ready())
            {
                ok = false;
            }
        }

        if (!ok || backwashAgents.empty())
        {

            CLogger::instance().error("CDrinkWaterInstallationTemplate::bind", "No correct backwash agents detected {}");
        }

        if (!backwashAgents.empty() && ok)
        {
            std::vector<SBackwashRecipe> recipes = collectRecipes(backwashAgents);
            if (!recipes.empty())
            {
                auto &candidate = backwashAgents.front();
                auto &recipe    = recipes.front();

                // null-guard заодно (см. п.5 из прошлого разбора) — без базы промывки не строим
                if (candidate.filters.empty() || !candidate.tank_before || !candidate.tank_after
                    || !candidate.recirculation_pump || !candidate.flow_reverser)
                {
                    ;
                }
                else
                {
                    uint32_t period_sec = 0;
                    for (auto &proc : undefined)
                    {
                        bool found = false;
                        for (auto & item : candidate.filters)
                        {
                            if (proc.owner == item && proc.spec.schedule_nature == SN_PROCEDURE)
                            {
                                period_sec = proc.spec.period_seconds;
                                found = true;
                                break;
                            }
                        }
                        if (found)
                            break;
                    }

                    if (period_sec == 0)
                    {
                        /*candidate.filter->set_error(EComponentError::ECE_WARNING);
                        candidate.filter->set_warnMessage(
                                "backwash: no TT_SCHEDULE/SN_PROCEDURE declared — NORMAL->PREP trigger missing");*/
                        ;
                    }
                    else
                    {
                        emit_backwash_sequence(model, normal_id, data, recipe, candidate, period_sec);
                    }
                }
            }
        }

        std::vector<SModelValidationIssue> issues = model->validate();
        if (!issues.empty())
        {
            for (auto & issue : issues)
            {
                // автор косяка
                Tstring issueAuthor = "issue[target_owner=";
                if (issue.target_owner)
                {
                    issueAuthor += issue.target_owner->schematicName() + " id:" + std::to_string(issue.target_owner->get_id());
                }
                issueAuthor += "; command_role: " + command_roles_str[issue.role] + "; desc: " + issue.description + "]\n";
                std::cerr << issueAuthor;
            }
        }

        model->print();

        return true;
    }

    float CDrinkWaterInstallationTemplate::resolve_tor_threshold(ESetpointSource source, const SCondition &cond) const
    {
        auto *tor = dynamic_cast<CDrinkWaterTor*>(m_subproject->general_tor());
        switch (source)
        {
            case ESS_TOR_OUT_PRESSURE:   return tor->water_out_press();
            case ESS_TOR_OUT_TURBIDITY: return tor->limit_at(cond.index)->limits()->front().right_limit;
            default: return 0.f; // сюда не должны попадать ESS_FIXED/ESS_SIGNAL — их раннер решает сам
        }
    }
}
