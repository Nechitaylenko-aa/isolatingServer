//
// Created by artem on 22.08.26.
//

//
// Created by artem on 22.08.26.
//

#include "IWaterTreatmentTemplate.h"
#include "CConductor.h"
#include "NComponent.h"
#include "CAutomationCollector.h"
#include "../../server/sources/logger-common/logger.h"
#include <CSubProject.h>
//#include "NComponent.h"
#include "codegen/CCodegenShadowsManager.h"
#include <cmath>

namespace NCore
{
    IWaterTreatmentTemplate::IWaterTreatmentTemplate()
    = default;

    IWaterTreatmentTemplate::~IWaterTreatmentTemplate()
    = default;



    std::vector<SBackwashCandidate>
    IWaterTreatmentTemplate::wire_filter_backwash_pattern(CSubProject *proj, const SCollectedAutomationData &data)
    {
        std::vector<SBackwashCandidate> result;
        std::vector<NComponent*> filters_seen;

        auto pred_valve2way = [](NComponent* comp)->bool
        {
            return comp->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_VALVE_WATER_TWO_WAY;
        };
        auto pred_valve_cut = [](NComponent* comp)->bool
        {
            return comp->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_VALVE_CUT;
        };
        auto pred_valve_cut_pump = [](NComponent* comp)->bool
        {
            auto type = comp->get_subtype();
            return type == (TComponentType)E_WATER_COMPONENTS::EWB_PUMP_STATION ||
                   type == (TComponentType)E_WATER_COMPONENTS::EWB_VALVE_CUT;
        };
        auto pred_pump = [](NComponent * comp)->bool
        {
            return comp->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_PUMP_STATION;
        };

        for (auto &cmd : data.commands)
        {
            if (cmd.owner->get_subtype() != (TComponentType)E_WATER_COMPONENTS::EWB_WATER_FILTER_ION_EXCHANGE &&
                cmd.owner->get_subtype() != (TComponentType)E_WATER_COMPONENTS::EWB_WATER_FILTER_SORPTION &&
                cmd.owner->get_subtype() != (TComponentType)E_WATER_COMPONENTS::EWB_WATER_FILTER_LIGHT
                    )
                continue;

            auto array = proj->project_cell()->get_components();

            if (std::find(filters_seen.begin(), filters_seen.end(), cmd.owner) != filters_seen.end())
                continue; // фильтр с 2 клапанами (backwash+drain) не задваиваем

            filters_seen.push_back(cmd.owner);

            SBackwashCandidate cand;

            for (auto &item : *array)
            {
                auto comp = dynamic_cast<NComponent*>(item);
                if (comp->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_WATER_FILTER_LIGHT ||
                    comp->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_WATER_FILTER_ION_EXCHANGE ||
                    comp->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_WATER_FILTER_SORPTION)
                {
                    cand.filters.push_back(comp);
                }
            }

            std::vector<CCell*> visited{ cmd.owner };
            cand.tank_before = find_first_boundary_upstream(cmd.owner->input(0), visited);

            visited = {cmd.owner};
            if (cand.tank_before)
            {
                auto pumps = find_components_upstream_matching(cand.tank_before->input(0), pred_valve_cut_pump, visited);
                if (!pumps.empty())
                    cand.filler_pump = pumps.front();
            }

            visited = {cmd.owner};
            if (cand.tank_before)
            {
                auto pumps = find_components_downstream_matching(cand.tank_before->output(0), pred_pump, visited);
                if (!pumps.empty())
                    cand.filter_pump = pumps.front();
            }



            visited = { cmd.owner };
            cand.tank_after = find_first_boundary_downstream(cmd.owner->output(0), visited);

            visited = { cmd.owner };
            auto pumps = find_components_downstream_matching(cand.tank_after->output(0), pred_pump, visited);
            if (!pumps.empty())
                cand.recirculation_pump = pumps.at(0);

            visited = { cmd.owner };
            auto valves_after = find_components_downstream_matching(cand.tank_after->outputs().at(0), pred_valve2way, visited);

            if (!valves_after.empty())
                cand.flow_reverser = valves_after.front();

            if (cand.flow_reverser)
            {
                visited = { cmd.owner };
                cand.valves_water = find_components_downstream_matching(cand.flow_reverser->output(1), pred_valve_cut, visited);
            }


            auto it = std::find_if(array->begin(), array->end(), [](CCell* item) {
                auto comp = dynamic_cast<NComponent*>(item);
                return comp->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_AIR_BLOWER;
            });
            if (it != array->end())
            {
                visited = { cmd.owner };
                cand.air_blower = dynamic_cast<NComponent*>(*it);
                cand.valves_air = find_components_downstream_matching(cand.air_blower->output(0), pred_valve_cut, visited);
            }

            result.push_back(cand);
        }
        return result;
    }

    uint16_t IWaterTreatmentTemplate::emit_backwash_sequence(
            CAutomationModel *model, uint16_t last_id,
            const SCollectedAutomationData &data,
            const SBackwashRecipe &r,
            const SBackwashCandidate &c,
            uint32_t trigger_period_sec)
    {
        uint16_t prep_id_1     = add_atom_set(model, last_id + 1, "BACKWASH_PREP_1");
        uint16_t prep_id_2     = add_atom_set(model, last_id + 2, "BACKWASH_PREP_2");
        uint16_t backwash_id = add_atom_set(model, last_id + 3, "BACKWASH");
        uint16_t restore_id  = add_atom_set(model, last_id + 4, "RESTORE");

        emit_stage_entry_snapshot(model, prep_id_1,     data, r.prep_overrides1);
        emit_stage_entry_snapshot(model, prep_id_2,     data, r.prep_overrides2);
        emit_stage_entry_snapshot(model, backwash_id, data, r.backwash_overrides);
        if (r.backwash_alternation.has_value())
            emit_alternation_atoms(model, backwash_id, data, r.backwash_alternation.value());
        emit_stage_entry_snapshot(model, restore_id,  data, r.restore_overrides);

        // backwash prepare stage 1 - fill capacity before
        SCondition cond1;
        cond1.role = ESignalRole::SR_LEVEL_UPPER;

        // ---- Переходы -----------------------------------------------------------
        // NORMAL -> PREP_1: раз в trigger_period_sec (из TT_SCHEDULE/SN_PROCEDURE фильтра)
        add_transition(model, last_id,
                       { prep_id_1, STrigger{ TRIG_TIME, {SCondition{}}, { (float)trigger_period_sec, 0, 0 } } });

        // PREP_1 -> PREP_2
        SCondition cond2{c.tank_before, SR_LEVEL_HH, 0, CMP_EQ, ESS_FIXED, 1.0f};
        add_transition(model, prep_id_1,
                       { prep_id_2, STrigger{ TRIG_EVENT, {cond2}, {}, } });

        // PREP_2 -> BACKWASH:
        SCondition prepDone{c.tank_after, SR_LEVEL_UPPER, 0, CMP_EQ, ESS_FIXED, 1.0f };
        add_transition(model, prep_id_2,
                       { backwash_id, STrigger{ TRIG_EVENT, {prepDone}, {} } });

        // BACKWASH -> RESTORE: задняя ёмкость наполнилась (== 1, не >= 0)
        SCondition backwash_done{ c.tank_after, SR_LEVEL_BOTT, 0, CMP_EQ, ESS_FIXED, 1 };
        add_transition(model, backwash_id,
                       { restore_id, STrigger{ TRIG_EVENT, {backwash_done}, {} } });

        // RESTORE -> NORMAL: задняя ёмкость опустела (== 1 на BOTT, не == 0)
        SCondition restore_done{ c.tank_after, SR_LEVEL_BOTT, 0, CMP_EQ, ESS_FIXED, 1 };
        add_transition(model, restore_id,
                       { last_id, STrigger{ TRIG_EVENT, {restore_done}, {} } });

        return prep_id_1;
    }

    NComponent *
    IWaterTreatmentTemplate::find_first_boundary_upstream(CCap *from_input_cap, std::vector<CCell *> &visited)
    {
        if (!from_input_cap) return nullptr;
        CConductor *pipe = from_input_cap->get_conductor(nullptr);
        if (!pipe) return nullptr;

        for (uint16_t i = 0; i < pipe->caps_amount(); ++i)
        {
            CCap *neighbor_cap = pipe->cap(i);
            if (!neighbor_cap || neighbor_cap == from_input_cap) continue;

            CCell *neighbor_cell = neighbor_cap->get_owner();
            if (!neighbor_cell || neighbor_cap->get_direction(nullptr) != CD_OUTPUT) continue;

            visited.push_back(neighbor_cell);
            auto *neighbor_comp = dynamic_cast<NComponent*>(neighbor_cell);
            if (!neighbor_comp) continue;

            if (neighbor_comp->is_flow_boundary())
                return neighbor_comp;

            for (uint16_t in = 0; in < neighbor_comp->countIn(); ++in)
                if (auto *b = find_first_boundary_upstream(neighbor_comp->input(in), visited))
                    return b;
        }
        return nullptr;
    }

    NComponent *
    IWaterTreatmentTemplate::find_first_boundary_downstream(CCap *from_output_cap, std::vector<CCell *> &visited)
    {
        if (!from_output_cap) return nullptr;
        CConductor *pipe = from_output_cap->get_conductor(nullptr);
        if (!pipe) return nullptr;

        for (uint16_t i = 0; i < pipe->caps_amount(); ++i)
        {
            CCap *neighbor_cap = pipe->cap(i);
            if (!neighbor_cap || neighbor_cap == from_output_cap) continue;

            CCell *neighbor_cell = neighbor_cap->get_owner();
            if (!neighbor_cell || neighbor_cap->get_direction(nullptr) != CD_INPUT) continue;

            visited.push_back(neighbor_cell);
            auto *neighbor_comp = dynamic_cast<NComponent*>(neighbor_cell);
            if (!neighbor_comp) continue;

            if (neighbor_comp->is_flow_boundary())
                return neighbor_comp;

            for (uint16_t out = 0; out < neighbor_comp->countOut(); ++out)
            {
                if (auto *b = find_first_boundary_downstream(neighbor_comp->output(out), visited))
                {
                    return b;
                }
            }
        }
        return nullptr;
    }

    void IWaterTreatmentTemplate::emit_stage_entry_snapshot(
            CAutomationModel *model, uint16_t stage_set_id,
            const SCollectedAutomationData &data,
            const std::vector<SStageOverride> &recipe_overrides)
    {
        SAutomationAtom entry;
        entry.trigger.kind = TRIG_TIME;
        entry.trigger.time = { 0, 0, 0 }; // разовое, сразу при входе

        for (auto &cmd : data.commands)
        {
            // явный override рецепта имеет приоритет
            auto it = std::find_if(recipe_overrides.begin(), recipe_overrides.end(),
                                   [&](auto &o)
                                   { return o.owner == cmd.owner &&
                                            o.role.role == cmd.role.role &&
                                            o.role.index == cmd.role.index; });

            bool activate = (it != recipe_overrides.end()) && it->activate; // тут было чтение флага default_activate
            add_actuator(model, cmd.anchor_cap, cmd.role);
            entry.actions.push_back({ cmd.owner, cmd.role, activate });
        }

        add_atom_to_set(model, stage_set_id, entry);
    }

    std::vector<SAutomationAtom> IWaterTreatmentTemplate::build_alternation_atoms(
            const SAlternatingAgent &agent)
    {
        std::vector<SAutomationAtom> atoms;

        // Активация primary/secondary больше НЕ пишется здесь вручную — см.
        // emit_alternation_atoms, где она идёт через ICodegenShadow самого компонента
        // (CShadowPumpStation/CShadowAirBlower), как и обычная ротация. Здесь остаётся
        // только координационная логика клапанов — она законно принадлежит установке,
        // не отдельному компоненту (клапан не имеет своей "внутренней логики").

        // --- Клапаны primary: открыть при статусе primary==1, закрыть при ==0 ---
        auto make_valve_atoms = [&](NComponent *status_owner, const SCommandRole &status_pump_role,
                                    const std::vector<std::pair<NComponent*, SCommandRole>> &valves,
                                    bool open_on_active)
        {
            for (auto activate_state : { true, false })
            {
                SAutomationAtom atom;
                atom.trigger.kind = TRIG_EVENT;
                SCondition cond;
                cond.signal_owner = status_owner;
                cond.role = SR_PUMP_STATUS;   // статус источника — насос/воздуходувка используют одну роль
                cond.index = 0;
                cond.op = CMP_EQ;
                cond.threshold_source = ESS_FIXED;
                cond.fixed_threshold = activate_state ? 1.0f : 0.0f;
                atom.trigger.conditions.push_back(cond);

                bool want_open = (activate_state == open_on_active);
                for (auto &[valve_owner, valve_role] : valves)
                    atom.actions.push_back({ valve_owner, valve_role, want_open });

                atom.origin = AO_INTERNAL;  // реакция на статус соседа, не boundary-каталог и не процедура
                atom.scope  = BRS_NORMAL;
                atoms.push_back(atom);
            }
        };

        make_valve_atoms(agent.primary,   agent.primary_role,   agent.primary_valves,   /*open_on_active*/true);
        make_valve_atoms(agent.secondary, agent.secondary_role, agent.secondary_valves, /*open_on_active*/true);

        return atoms;
    }

    void IWaterTreatmentTemplate::emit_alternation_atoms(
            CAutomationModel *model, uint16_t stage_set_id,
            const SCollectedAutomationData &data,
            const SAlternatingAgent &agent)
    {
        // регистрация актуаторов — как в emit_stage_entry_snapshot, только явно по списку участников
        add_actuator(model, agent.primary->input(0),   agent.primary_role);
        add_actuator(model, agent.secondary->input(0), agent.secondary_role);
        for (auto &[owner, role] : agent.primary_valves)
            add_actuator(model, owner->input(0), role);
        for (auto &[owner, role] : agent.secondary_valves)
            add_actuator(model, owner->input(0), role);

        // Активация primary/secondary — через собственную кодоген-тень каждого компонента,
        // не переизобретаем её ротацию/выбор юнита здесь второй раз (см. чат: "мы сделали
        // два параллельных пути"). Тень сама решает, какой физический юнит внутри себя
        // включить — снаружи нужен только факт "активируйся" в нужной фазе.
        float full_period = (float)agent.period_sec * 2.0f;

        struct SPhaseParticipant { NComponent *owner; SCommandRole role; float offset; };
        SPhaseParticipant participants[2] = {
                { agent.primary,   agent.primary_role,   0.0f },
                { agent.secondary, agent.secondary_role, (float)agent.period_sec },
        };

        for (auto &p : participants)
        {
            auto *shadow = CCodegenShadowsManager::codegen_shadow_instance(p.owner);
            if (!shadow)
                continue;

            SBoundaryInterlockCandidate start_cand;
            start_cand.target_owner  = p.owner;
            start_cand.reaction_role = p.role;
            start_cand.activate      = true;
            start_cand.scope         = BRS_NORMAL;
            start_cand.trigger       = STrigger{ TRIG_TIME, {},
                                                 STimeTrigger{ p.offset, full_period, (float)agent.period_sec } };

            SBoundaryInterlockCandidate stop_cand = start_cand;
            stop_cand.activate = false;
            stop_cand.trigger  = STrigger{ TRIG_TIME, {},
                                           STimeTrigger{ std::fmod(p.offset + (float)agent.period_sec, full_period),
                                                         full_period, (float)agent.period_sec } };

            std::vector<SBoundaryInterlockCandidate> phase_candidates{ start_cand, stop_cand };

            for (auto &atom : shadow->build_internal_topology(phase_candidates))
                add_atom_to_set(model, stage_set_id, atom);
        }

        for (auto &atom : build_alternation_atoms(agent))
            add_atom_to_set(model, stage_set_id, atom);
    }

    std::vector<SBackwashRecipe>
    IWaterTreatmentTemplate::collectRecipes(const std::vector<SBackwashCandidate> &backwashAgents)
    {
        std::vector<SBackwashRecipe> recipes;

        if (backwashAgents.empty()) return recipes;

        auto agent = backwashAgents.at(0);

        if (!agent.is_ready())
        {
            CLogger::instance().error("IWaterTreatmentTemplate::collectRecipes", "backwashAgent not ready!");
            return {};
        }

        SBackwashRecipe recipe;

        recipe.prep_overrides1 = {
                { agent.filler_pump, agent.filler_pump->required_commands().front(), true },
                {agent.filler_pump, agent.filter_pump->required_commands().front(), false}
        };

        recipe.prep_overrides2 = {
                { agent.filler_pump, agent.filler_pump->required_commands().front(), false },
                { agent.filter_pump, agent.filter_pump->required_commands().front(), true },
                {agent.recirculation_pump, agent.recirculation_pump->required_commands().front(), false}
        };

        recipe.backwash_overrides = {
                { agent.flow_reverser, SCommandRole{CR_FLOW_DIRECTION_VALVE, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, ECapSemantics::CS_COMPONENT_SCOPED,
                                                    false, 0}, true },
                {agent.recirculation_pump, agent.recirculation_pump->required_commands().front(), true}
        };

        if (!agent.air_blower)
        {
            recipe.backwash_overrides.push_back({ agent.recirculation_pump, SCommandRole{CR_PUMP_START_STOP, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, CS_COMPONENT_SCOPED,
                                                                                         false, 0},  true });
        }

        if (agent.air_blower && !agent.valves_air.empty() && !agent.valves_water.empty())
        {
            collect_air_cut_recipe(agent, recipe);
            recipe.restore_overrides.push_back(
                    { agent.air_blower, SCommandRole{CR_BLOWER_START_STOP, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, ECapSemantics::CS_COMPONENT_SCOPED,
                                                        false, 0}, false }
                    );
        }

        for (auto &item : agent.filters)
        {

            SStageOverride ovr0{item, SCommandRole{CR_FLOW_DIRECTION_VALVE, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, CS_COMPONENT_SCOPED,
                                                   false, 0}, true};
            SStageOverride ovr1{item, SCommandRole{CR_FLOW_DIRECTION_VALVE, 1, EMU_AMOUNT, EMUAMO::EA_ENUM, CS_COMPONENT_SCOPED,
                                                   false, 0}, true};
            recipe.backwash_overrides.push_back(ovr0);
            recipe.backwash_overrides.push_back(ovr1);
        }

        recipe.restore_overrides = {
                { agent.flow_reverser, SCommandRole{CR_FLOW_DIRECTION_VALVE, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, ECapSemantics::CS_COMPONENT_SCOPED,
                                                    false, 0}, false },
        };

        recipes.push_back(recipe);

        return recipes;
    }

    void IWaterTreatmentTemplate::collect_air_cut_recipe(SBackwashCandidate &candidate, SBackwashRecipe &recipe)
    {
        recipe.backwash_alternation = SAlternatingAgent{
                .primary   = candidate.recirculation_pump,
                .primary_role   = SCommandRole{CR_PUMP_START_STOP, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, ECapSemantics::CS_COMPONENT_SCOPED,
                                               false, 0},
                .secondary = candidate.air_blower,
                .secondary_role =SCommandRole{CR_PUMP_START_STOP, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, ECapSemantics::CS_COMPONENT_SCOPED,
                                              false, 0},
                .period_sec = 60,

        };
        for (auto & airV : candidate.valves_air)
        {
            recipe.backwash_alternation->secondary_valves.emplace_back(airV, SCommandRole{CR_VALVE_OPEN_CLOSE, 0, EMU_AMOUNT, EA_ENUM,
                                                              CS_COMPONENT_SCOPED, false, 0}
                    );
        }
        for (auto & waterW : candidate.valves_water)
        {
            recipe.backwash_alternation->primary_valves.emplace_back(waterW, SCommandRole{CR_VALVE_OPEN_CLOSE, 0, EMU_AMOUNT, EA_ENUM,
                                                                                          CS_COMPONENT_SCOPED, false, 0});
        }
    }

} // NCore

