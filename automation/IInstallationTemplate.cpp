
#include "IInstallationTemplate.h"
#include "CAutomationModel.h"
#include "CAutomationCollector.h"
#include "CCap.h"
#include "CCell.h"
#include "NComponent.h"
#include "CDrinkWaterInstallationTemplate.h"
#include <algorithm>
#include "CSubProject.h"
#include "codegen/CCodegenShadowsManager.h"

namespace NCore
{
    IInstallationTemplate* IInstallationTemplate::create(const E_PROJECT_TYPE &type)
    {
        // TODO: не видел оригинальной реализации в этой сессии — подставь реальный
        // switch по всем поддерживаемым E_PROJECT_TYPE (очистка стоков, ГРС и т.п.),
        // здесь только тот случай, что уже встречался в контексте (питьевая вода).
        switch (type)
        {
            case pt_water_drink: // TODO: сверить точное имя константы с core-types.h
                return new CDrinkWaterInstallationTemplate();
            default:
                return nullptr;
        }
    }

    std::vector<NComponent*> IInstallationTemplate::find_components_upstream_with_command(
            CCap *from_input_cap, ECommandRole role, std::vector<CCell*> &visited)
    {
        return find_components_upstream_matching(from_input_cap,
                                                 [role](NComponent *c) {
                                                     for (auto &cmd : c->required_commands())
                                                         if (cmd.role == role) return true;
                                                     return false;
                                                 }, visited);
    }



    std::vector<NComponent *>
    IInstallationTemplate::find_components_downstream_with_command(CCap *from_output_cap, ECommandRole role,
                                                                   std::vector<CCell *> &visited)
    {
        return find_components_downstream_matching(from_output_cap,
                                                 [role](NComponent *c) {
                                                     for (auto &cmd : c->required_commands())
                                                         if (cmd.role == role) return true;
                                                     return false;
                                                 }, visited);
    }


    std::vector<SBoundaryInterlockCandidate> IInstallationTemplate::wire_boundary_patterns(
            CSubProject *proj,
            const SCollectedAutomationData &data)
    {
        std::vector<SBoundaryInterlockCandidate> candidates;

        for (const auto &sig : data.signals)
        {
            if (!sig.owner)
                continue;

            for (const auto &rule : kBoundaryReactionCatalog)
            {
                if (sig.role.role != rule.trigger_role)
                    continue;

                Tstring source = sig.owner->schematicName();

                if (rule.direction == EBoundarySearchDirection::BSD_UPSTREAM)
                {

                    for (uint16_t in = 0; in < sig.owner->countIn(); ++in)
                    {
                        std::vector<CCell*> visited{ sig.owner };
                        CCap *input = sig.owner->input(in);

                        if (input->get_contour() == CT_ADDITIONAL)
                            continue;


                        auto targets = find_components_upstream_with_command(
                                input, rule.reaction_role, visited);

                        //assert(!targets.empty()); // ошибка схемы
                        /*if (targets.empty())
                        {
                            sig.owner->set_error(EComponentError::ESE_ACTUATOR);
                        }*/

                        for (auto *target : targets)
                        {
                            SCommandRole cr{
                                .role = rule.reaction_role,
                                .index = sig.role.index,
                                .unit = sig.role.unit,
                                .subtype = sig.role.subtype,
                                .cap_semantics = sig.role.cap_semantics,
                                .is_input = sig.role.is_input,
                                .cap_index = sig.role.cap_index,
                            };
                            SCondition gate;
                            gate.signal_owner = sig.owner;
                            gate.role = sig.role.role;
                            gate.index = sig.role.index;
                            gate.op = CMP_EQ;
                            gate.threshold_source = ESS_FIXED;
                            gate.fixed_threshold = 1.0f;

                            SBoundaryInterlockCandidate candidate{
                                    .signal_owner  = sig.owner,
                                    .signal_role   = sig.role,
                                    .target_owner  = target,
                                    .reaction_role = cr,
                                    .activate      = rule.activate,
                                    .scope         = rule.scope,
                                    .trigger       = STrigger{ TRIG_EVENT, { gate }, {} }
                            };
                            candidates.push_back(candidate);
                        }
                    }
                }
                else
                {
                    for (uint16_t out = 0; out < sig.owner->countOut(); ++out)
                    {
                        std::vector<CCell*> visited{ sig.owner };
                        auto * cap = sig.owner->output(out);

                        if (cap->get_contour() == CT_ADDITIONAL)
                            continue;

                        auto targets = find_components_downstream_with_command(
                                cap, rule.reaction_role, visited);

                        //assert(!targets.empty()); // ошибка схемы
                        if (targets.empty())
                            sig.owner->set_error(EComponentError::ESE_ACTUATOR);

                        for (auto *target : targets)
                        {
                            SCommandRole cr{
                                    .role = rule.reaction_role,
                                    .index = sig.role.index,
                                    .unit = sig.role.unit,
                                    .subtype = sig.role.subtype,
                                    .cap_semantics = sig.role.cap_semantics,
                                    .is_input = sig.role.is_input,
                                    .cap_index = sig.role.cap_index,
                            };

                            SCondition gate;
                            gate.signal_owner = sig.owner;
                            gate.role = sig.role.role;
                            gate.index = sig.role.index;
                            gate.op = CMP_EQ;
                            gate.threshold_source = ESS_FIXED;
                            gate.fixed_threshold = 1.0f;

                            SBoundaryInterlockCandidate candidate{
                                    .signal_owner  = sig.owner,
                                    .signal_role   = sig.role,
                                    .target_owner  = target,
                                    .reaction_role = cr,
                                    .activate      = rule.activate,
                                    .scope         = rule.scope,
                                    .trigger       = STrigger{ TRIG_EVENT, { gate }, {} }
                            };
                            candidates.push_back(candidate);
                        }
                    }
                }
            }
        }

        return candidates;
    }

    // ---- Посредники к приватным сеттерам CAutomationModel -----------------------------

    Instrument* IInstallationTemplate::add_instrument(CAutomationModel *model, CCap *anchor, const SSignalRole &role)
    {
        return model->add_instrument(anchor, role);
    }

    Instrument* IInstallationTemplate::add_service_instrument(CAutomationModel *model, const SSignalRole &role,
                                                              std::function<float()> source)
    {
        return model->add_service_instrument(role, std::move(source));
    }

    Actuator* IInstallationTemplate::add_actuator(CAutomationModel *model, CCap *anchor, const SCommandRole &role)
    {
        return model->add_actuator(anchor, role);
    }

    uint16_t IInstallationTemplate::add_atom_set(CAutomationModel *model, uint16_t id, const Tstring &display_name)
    {
        return model->add_atom_set(id, display_name);
    }

    void IInstallationTemplate::ensure_instruments_for_trigger(CAutomationModel *model, const STrigger &trigger)
    {
        if (trigger.kind != TRIG_EVENT)
            return; // TRIG_TIME/TRIG_NO_CONDITIONS не читают сигналы

        for (auto &cond : trigger.conditions)
        {
            if (cond.signal_owner && is_state_derived_role(cond.role))
            {
                add_internal_instrument(model, cond.signal_owner,
                        SSignalRole{ cond.role, cond.index, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                     EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
            }

            // сравнение сигнал-к-сигналу (ротация по SR_MOTO_HOURS и т.п.) — второй операнд
            // тоже нуждается в инструменте, если он читается со state-derived роли
            if (cond.threshold_source == ESS_SIGNAL && cond.compare_owner && is_state_derived_role(cond.compare_role))
            {
                add_internal_instrument(model, cond.compare_owner,
                        SSignalRole{ cond.compare_role, cond.compare_index, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                     EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
            }
        }
    }

    void IInstallationTemplate::add_atom_to_set(CAutomationModel *model, uint16_t set_id, const SAutomationAtom &atom)
    {
        ensure_instruments_for_trigger(model, atom.trigger);
        model->add_atom_to_set(set_id, atom);
    }

    void IInstallationTemplate::add_transition(CAutomationModel *model, uint16_t set_id, const STransition &transition)
    {
        ensure_instruments_for_trigger(model, transition.trigger);
        model->add_transition(set_id, transition);
    }

    void IInstallationTemplate::add_supervisory_atom(CAutomationModel *model, const SAutomationAtom &atom)
    {
        ensure_instruments_for_trigger(model, atom.trigger);
        model->add_supervisory_atom(atom);
    }

    void IInstallationTemplate::set_initial_active_set(CAutomationModel *model, uint16_t id)
    {
        model->set_initial_active_set(id);
    }

    std::vector<SDeclaredAutomation> IInstallationTemplate::wire_schedule_atoms(CAutomationModel *model,
                                                                                const SCollectedAutomationData &data,
                                                    uint16_t target_set_id)
    {
        bool ok = true;
        std::vector<SDeclaredAutomation> undeclared_procedures;
        for (auto &a : data.automations)
        {
            if (a.spec.trigger != TT_SCHEDULE || a.spec.schedule_nature != SN_DUTY_CYCLE)
            {
                undeclared_procedures.push_back(a);
                continue; // PROCEDURE намеренно не трогаем здесь
            }

            if (a.spec.duration_seconds == 0 || a.spec.duration_seconds >= a.spec.period_seconds)
            {
                a.owner->set_error(EComponentError::ECE_CRITICAL);
                a.owner->set_warnMessage("SN_DUTY_CYCLE requires 0 < duration_seconds < period_seconds");
                ok = false;
                continue; // не материализуем невалидную пару атомов вообще
            }

            for (auto &cmd_role : a.spec.output_roles)
            {
                SAutomationAtom on;
                on.trigger.kind = TRIG_TIME;
                on.trigger.time = { 0, (float)a.spec.period_seconds, (float)a.spec.duration_seconds };
                on.actions.push_back({ a.owner, cmd_role, /*activate*/ true });
                on.origin = AO_SCHEDULE;
                add_atom_to_set(model, target_set_id, on);

                SAutomationAtom off;
                off.trigger.kind = TRIG_TIME;
                off.trigger.time = { (float)a.spec.duration_seconds,
                                     (float)a.spec.period_seconds,
                                     (float)(a.spec.period_seconds - a.spec.duration_seconds) };
                off.actions.push_back({ a.owner, cmd_role, /*activate*/ false });
                off.origin = AO_SCHEDULE;
                add_atom_to_set(model, target_set_id, off);
            }
        }

        return undeclared_procedures;
    }

    std::vector<NComponent *> IInstallationTemplate::find_components_upstream_matching(CCap *from_input_cap,
                                                                                       const IInstallationTemplate::CComponentPredicate &pred,
                                                                                       std::vector<CCell *> &visited)
    {

        std::vector<NComponent*> found;
        if (!from_input_cap) return found;

        CConductor *pipe = from_input_cap->get_conductor(nullptr);
        if (!pipe) return found;

        for (uint16_t i = 0; i < pipe->caps_amount(); ++i)
        {
            CCap *neighbor_cap = pipe->cap(i);

            if (!neighbor_cap || neighbor_cap == from_input_cap) continue;

            if (neighbor_cap->get_contour() == CT_ADDITIONAL)
                continue;

            CCell *neighbor_cell = neighbor_cap->get_owner();
            if (!neighbor_cell) continue;
            if (neighbor_cap->get_direction(nullptr) != CD_OUTPUT) continue;

            visited.push_back(neighbor_cell);
            auto *neighbor_comp = dynamic_cast<NComponent*>(neighbor_cell);
            if (!neighbor_comp) continue;

            if (pred(neighbor_comp))
            {
                found.push_back(neighbor_comp);
                continue; // нашли — по этой ветке дальше не идём
            }

            if (!neighbor_comp->is_flow_boundary())
            {
                for (uint16_t in = 0; in < neighbor_comp->countIn(); ++in)
                {
                    auto *cap = neighbor_comp->input(in);
                    if (cap->get_contour() ==CT_ADDITIONAL)
                        continue;
                    auto deeper = find_components_upstream_matching(cap, pred, visited);
                    found.insert(found.end(), deeper.begin(), deeper.end());
                }
            }
        }
        return found;
    }

    std::vector<NComponent *> IInstallationTemplate::find_components_downstream_matching(CCap *from_output_cap,
                                                                                         const IInstallationTemplate::CComponentPredicate &pred,
                                                                                         std::vector<CCell *> &visited)
    {
        std::vector<NComponent*> found;
        if (!from_output_cap) return found;

        CConductor *pipe = from_output_cap->get_conductor(nullptr);
        if (!pipe) return found;

        for (uint16_t i = 0; i < pipe->caps_amount(); ++i)
        {
            CCap *neighbor_cap = pipe->cap(i);
            if (!neighbor_cap || neighbor_cap == from_output_cap) continue;

            if (neighbor_cap->get_contour() == CT_ADDITIONAL)
                continue;

            CCell *neighbor_cell = neighbor_cap->get_owner();
            if (!neighbor_cell) continue;
            if (neighbor_cap->get_direction(nullptr) != CD_INPUT) continue;

            visited.push_back(neighbor_cell);
            auto *neighbor_comp = dynamic_cast<NComponent*>(neighbor_cell);
            if (!neighbor_comp) continue;

            if (pred(neighbor_comp))
            {
                found.push_back(neighbor_comp);
                continue; // нашли — по этой ветке дальше не идём
            }

            if (!neighbor_comp->is_flow_boundary())
            {
                for (uint16_t out = 0; out < neighbor_comp->countOut(); ++out)
                {
                    auto * cap = neighbor_comp->output(out);
                    if (cap->get_contour() == CT_ADDITIONAL) continue;
                    auto deeper = find_components_downstream_matching(cap, pred, visited);
                    found.insert(found.end(), deeper.begin(), deeper.end());
                }
            }
        }
        return found;
    }

    static void dfs_classify(CCap *from_cap, std::vector<CCell*> &on_stack,
                             std::vector<CCell*> &visited,
                             std::vector<SAlternateRoute> &alternates)
    {
        CConductor *pipe = from_cap->get_conductor(nullptr);
        if (!pipe) return;

        for (uint16_t i = 0; i < pipe->caps_amount(); ++i)
        {
            CCap *neighbor_cap = pipe->cap(i);
            if (!neighbor_cap || neighbor_cap == from_cap) continue;

            CCell *neighbor_cell = neighbor_cap->get_owner();
            if (!neighbor_cell) continue;

            auto *neighbor_comp = dynamic_cast<NComponent*>(neighbor_cell);
            if (!neighbor_comp) continue;

            bool already_seen = std::find(visited.begin(), visited.end(), neighbor_cell) != visited.end();

            if (already_seen)
            {
                // back edge — не важно, ведёт ли на предка в стеке (байпас насоса)
                // или на произвольный уже пройденный узел (разворот через водораздел) —
                // в обоих случаях это НЕ дефолтный ход рабочего тела.
                //alternates.push_back({ neighbor_comp, neighbor_cap });
                continue; // дальше по этой ветке не идём — она уже классифицирована
            }

            visited.push_back(neighbor_cell);
            on_stack.push_back(neighbor_cell);

            for (uint16_t out = 0; out < neighbor_comp->countOut(); ++out)
                dfs_classify(neighbor_comp->output(out), on_stack, visited, alternates);

            on_stack.pop_back();
        }
    }

    /*std::vector<SAlternateRoute> IInstallationTemplate::detect_alternate_routes(CSubProject *proj)
    {
        std::vector<SAlternateRoute> alternates;
        std::vector<CCell*> visited, on_stack;

        CCell *root = proj->project_cell();
        // старт — главный вход проекта (Cap[2]/Cap[3] по твоей топологии, направление OUT изнутри = поток внутрь)
        for (auto *cap : root->get_base_outputs())  // либо конкретный m_generalTor-driven inlet — сверить сигнатуру
        {
            visited.push_back(root);
            on_stack.push_back(root);
            dfs_classify(cap, on_stack, visited, alternates);
        }

        m_alternate_routes = alternates;

        return alternates;
    }*/

    Instrument* IInstallationTemplate::add_internal_instrument(CAutomationModel *model, NComponent *owner, const SSignalRole &role)
    { return model->add_internal_instrument(owner, role); }

    Actuator* IInstallationTemplate::add_internal_actuator(CAutomationModel *model, NComponent *owner, const SCommandRole &role)
    { return model->add_internal_actuator(owner, role); }

    void IInstallationTemplate::add_internal_variable(CAutomationModel *model, SVariableBehaviorSpec spec)
    {
        // moto_hours (и т.п.) читают condition_role в advance_variables() независимо от
        // атомов/триггеров — ensure_instruments_for_trigger сюда не дотягивается (нет
        // STrigger), поэтому регистрация инструмента для condition_role дублируется тут.
        if (spec.variable.owner && is_state_derived_role(spec.condition_role))
        {
            add_internal_instrument(model, spec.variable.owner,
                    SSignalRole{ spec.condition_role, spec.condition_index, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                                 EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 });
        }

        model->add_internal_variable(std::move(spec));
    }

    void IInstallationTemplate::add_aggregate_signal(CAutomationModel *, NComponent *, const SAggregateSignalSpec &)
    {

    }
}
