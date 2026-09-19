//
// ВНИМАНИЕ: этот .cpp писался без доступа к предыдущей рабочей версии файла (она не
// была приложена в этой сессии) — реконструирован из комментариев в CAutomationModel.h
// и automation_layer_*.md. Места, где я предполагаю сигнатуру CSubProject, помечены
// TODO ниже — подставь реальные имена методов перед сборкой, если они отличаются.
//

#include "CAutomationModel.h"
#include "actuator/Instrument.h"
#include "actuator/Actuator.h"
#include "actuator/CServiceInstrument.h"
#include "CAutomationCollector.h"
#include "IInstallationTemplate.h"
#include <algorithm>
#include "IGeneralTor.h"
#include <iostream>
#include <NComponent.h>
#include "actuator/CInternalInstrument.h"

// TODO: подставь реальный путь/заголовок CSubProject, если отличается.
#include "CSubProject.h"

namespace NCore
{
    CAutomationModel::CAutomationModel(CSubProject *owner)
            : m_owner(owner)
    {
    }

    CAutomationModel::~CAutomationModel()
    {
        clear();
        delete m_tmpl;
    }

    void CAutomationModel::clear()
    {
        for (auto *instr: m_instruments) delete instr;
        for (auto *act: m_actuators) delete act;

        m_instruments.clear();
        m_actuators.clear();
        m_atom_sets.clear();
        m_supervisory_atoms.clear();
        m_active_set_id = 0;
    }

    bool CAutomationModel::bind()
    {
        clear();

        if (!m_owner)
            return false;

        CCell *root = m_owner->project_cell();
        if (!root)
        {
            return false;
        }

        SCollectedAutomationData data = CAutomationCollector::collect(root);

        E_PROJECT_TYPE type = m_owner->general_tor()->type();

        delete m_tmpl;
        m_tmpl = IInstallationTemplate::create(type);
        if (!m_tmpl)
        {
            return false;
        }

        bool ok = m_tmpl->bind(m_owner, data);


        if (ok)
        {
            if (m_cbOnBind)
                m_cbOnBind();
        }

        return ok;
    }

    std::vector<Instrument *> CAutomationModel::instruments() const
    {
        return m_instruments;
    }

    std::vector<Actuator *> CAutomationModel::actuators() const
    {
        return m_actuators;
    }

    const std::vector<SAtomSet> &CAutomationModel::atom_sets() const
    {
        return m_atom_sets;
    }

    const std::vector<SAutomationAtom> &CAutomationModel::supervisory_atoms() const
    {
        return m_supervisory_atoms;
    }

    uint16_t CAutomationModel::active_set_id() const
    {
        return m_active_set_id;
    }

    Instrument *CAutomationModel::add_instrument(CCap *anchor, const SSignalRole &role)
    {
        for (auto *existing : m_instruments)
        {
            if (auto *phys = dynamic_cast<CPhysicalInstrument*>(existing))
            {
                if (phys->anchor() == anchor
                    && phys->role().role == role.role
                    && phys->role().index == role.index)
                {
                    return phys; // уже существует — переиспользуем
                }
            }
        }

        auto *instr = new CPhysicalInstrument(anchor, role);
        m_instruments.push_back(instr);
        return instr;
    }

    Instrument *CAutomationModel::add_service_instrument(const SSignalRole &role, std::function<float()> source)
    {

        auto *instr = new CServiceInstrument(role, std::move(source));
        m_instruments.push_back(instr);
        return instr;
    }

    Actuator *CAutomationModel::add_actuator(CCap *anchor, const SCommandRole &role)
    {
        for (auto *existing : m_actuators)
        {
            if (existing->anchor() == anchor
                && existing->role().role == role.role
                && existing->role().index == role.index)
            {
                return existing; // уже существует — переиспользуем
            }
        }

        auto *act = new Actuator(anchor, role);
        m_actuators.push_back(act);
        return act;
    }

    uint16_t CAutomationModel::add_atom_set(uint16_t id, const std::string &display_name)
    {
        SAtomSet set;
        set.id = m_atom_sets.size();
        set.display_name = display_name;
        m_atom_sets.push_back(set);

        return set.id;
    }

    SAtomSet *CAutomationModel::find_atom_set(uint16_t set_id)
    {
        for (auto &set: m_atom_sets)
        {
            if (set.id == set_id)
                return &set;
        }
        return nullptr;
    }

    void CAutomationModel::add_atom_to_set(uint16_t set_id, const SAutomationAtom &atom)
    {
        SAtomSet *set = find_atom_set(set_id);
        if (!set)
            return; // TODO: набор не создан заранее add_atom_set — решить, логировать ли это как ошибку резолвера

        set->atoms.push_back(atom);
    }

    void CAutomationModel::add_transition(uint16_t set_id, const STransition &transition)
    {
        SAtomSet *set = find_atom_set(set_id);
        if (!set)
            return;

        set->transitions.push_back(transition);
    }

    void CAutomationModel::add_supervisory_atom(const SAutomationAtom &atom)
    {
        m_supervisory_atoms.push_back(atom);
    }

    void CAutomationModel::set_initial_active_set(uint16_t id)
    {
        m_active_set_id = id;
    }

    static const char *compare_op_str(NCore::ECompareOp op)
    {
        switch (op)
        {
            case NCore::CMP_LT:
                return "<";
            case NCore::CMP_LE:
                return "<=";
            case NCore::CMP_GT:
                return ">";
            case NCore::CMP_GE:
                return ">=";
            case NCore::CMP_EQ:
                return "==";
            case NCore::CMP_NE:
                return "!=";
        }
        return "?";
    }

    static void print_condition(const SCondition &cond)
    {
        std::cout << "signal[role=" << signal_roles_str[cond.role]
                  << " idx=" << cond.index << " source: ("
                  << (cond.signal_owner ? cond.signal_owner->schematicName() : Tstring("?"))
                  << " id:" << (cond.signal_owner ? std::to_string(cond.signal_owner->get_id()) : "?")
                  << ")] " << compare_op_str(cond.op) << " ";

        if (cond.threshold_source == ESS_SIGNAL)
            std::cout << "signal[role=" << signal_roles_str[cond.compare_role]
                      << " idx=" << cond.compare_index << " of ("
                      << (cond.compare_owner ? cond.compare_owner->schematicName() : Tstring("?")) << ")]";
        else
            std::cout << cond.fixed_threshold;
    }

    void CAutomationModel::print() const
    {
        std::cout << "========== CAutomationModel ==========\n";

        std::cout << "-- Instruments (" << m_instruments.size() << ") --\n";
        for (auto *instr: m_instruments)
        {
            std::cout << "  tag=" << instr->tag()
                      << " role=" << signal_roles_str[instr->role().role]
                      << " idx=" << instr->role().index
                      << " io=" << instr->io_address() << "\n";
        }

        std::cout << "-- Actuators (" << m_actuators.size() << ") --\n";
        for (auto *act: m_actuators)
        {
            std::cout << "  role=" << command_roles_str[act->role().role]
                      << " idx=" << act->role().index
                      << " tag=" << act->tag()  << "\n";
        }

        std::cout << "-- Supervisory atoms (" << m_supervisory_atoms.size() << ") --\n";

        for (auto &atom: m_supervisory_atoms)
            print_atom(atom, "  ");

        std::cout << "-- Atom sets (" << m_atom_sets.size() << ") --\n";

        for (auto &set: m_atom_sets)
        {
            std::cout << "  [" << set.id << "] \"" << set.display_name << "\""
                      << (set.id == m_active_set_id ? "  <-- active" : "") << "\n";

            for (auto &atom: set.atoms)
                print_atom(atom, "    ");

            for (auto &tr: set.transitions)
            {
                std::cout << "    -> to_set=" << tr.to_set_id << "  TRIGGER: ";
                if (tr.trigger.kind == TRIG_TIME)
                {
                    std::cout << "after " << tr.trigger.time.offset_from_entry_sec << "s\n";
                }
                else
                {
                    for (size_t i = 0; i < tr.trigger.conditions.size(); ++i)
                    {
                        if (i > 0) std::cout << " AND ";
                        print_condition(tr.trigger.conditions[i]);
                    }
                    std::cout << "\n";
                }
            }
        }

        std::cout << "Variables========" << std::endl;
        static const char* variable_behavior_str[] = { "NONE", "ACCUMULATE_WHILE_TRUE" }; // расширяется вместе с EVariableBehavior

        std::cout << "-- Internal variables (" << m_variables.size() << ") --\n";
        for (auto &vb : m_variables)
        {
            std::cout << "  " << vb.variable.name
                      << " [owner=" << (vb.variable.owner
                                            ? vb.variable.owner->schematicName() + " id:" + std::to_string(vb.variable.owner->get_id())
                                            : Tstring("?"))
                      << ", retain=" << (vb.variable.retain ? "yes" : "no") << "]\n";

            std::cout << "      " << variable_behavior_str[vb.behavior]
                      << " while signal[role=" << signal_roles_str[vb.condition_role]
                      << " idx=" << vb.condition_index << "] is true\n";

            std::cout << "      reset: " << (vb.has_reset
                                                 ? ("cmd[role=" + command_roles_str[vb.reset_role] + " idx=" + std::to_string(vb.reset_index) + "]")
                                                 : Tstring("none — накапливается без сброса"))
                      << ", max=" << vb.max_value << "\n";
        }

        std::cout << "=======================================\n";
    }


    void CAutomationModel::print_atom(const SAutomationAtom &atom, const std::string &indent)
    {
        static const char* origin_str[] = { "BOUNDARY", "SCHEDULE", "MANUAL", "INTERNAL" }; // добавлен AO_INTERNAL

        std::cout << indent << "[" << origin_str[atom.origin] << "] IF ";

        if (atom.trigger.kind == TRIG_EVENT)
        {
            if (atom.trigger.conditions.empty())
            {
                std::cout << "(нет условий — TRIG_NO_CONDITIONS-подобный случай, всегда true)";
            }
            else
            {
                for (size_t i = 0; i < atom.trigger.conditions.size(); ++i)
                {
                    if (i > 0) std::cout << " AND ";
                    print_condition(atom.trigger.conditions[i]);
                }
            }
        }
        else // TRIG_TIME — без изменений
        {
            const auto &t = atom.trigger.time;
            if (t.period_sec <= 0)
            {
                std::cout << "time_since_entry >= " << t.offset_from_entry_sec << "s (once)";
            }
            else
            {
                float window_end = t.offset_from_entry_sec + t.duration_sec;
                std::cout << "phase in [" << t.offset_from_entry_sec << "s, " << window_end << "s) "
                          << "of period=" << t.period_sec << "s"
                          << " (duration=" << t.duration_sec << "s)";
                if (t.duration_sec <= 0)
                    std::cout << "  ** WARNING: empty window, atom never fires **";
                else if (window_end > t.period_sec)
                    std::cout << "  ** WARNING: window exceeds period, wraps unexpectedly **";
            }
        }

        std::cout << " THEN\n";
        for (auto &action: atom.actions)
        {
            Tstring act = action.activate ?  "YES" : "NO";

            std::cout << indent << "    " << (action.activate ? "START/OPEN " : "STOP/CLOSE ")
                      << "cmd[role=" << command_roles_str[action.command.role]
                      << " idx=" << action.command.index
                      << " target=" << (action.target_owner ? action.target_owner->schematicName() + " id:" + std::to_string(action.target_owner->get_id()) : "?")
                      << " activate= " << act
                      << "]\n";
        }
    }

    bool CAutomationModel::evaluate_time(const STimeTrigger &t, float t_since_entry)
    {
        if (t.period_sec <= 0)
            return t_since_entry >= t.offset_from_entry_sec; // разовое — переход меняет набор, повторно не сработает

        float phase = std::fmod(t_since_entry, t.period_sec);
        return phase >= t.offset_from_entry_sec
               && phase <  t.offset_from_entry_sec + t.duration_sec;
    }

    std::vector<SModelValidationIssue> CAutomationModel::validate() const
    {
        std::vector<SModelValidationIssue> issues;

        for (auto &set : m_atom_sets)
        {
            std::map<std::tuple<NComponent*, ECommandRole, uint16_t>,
                    std::vector<std::pair<const SAutomationAtom*, const SAtomCommand*>>> groups;

            for (auto &atom : set.atoms)
                for (auto &action : atom.actions)
                    groups[{action.target_owner, action.command.role, action.command.index}]
                            .push_back({&atom, &action});

            for (auto &[key, entries] : groups)
            {
                if (entries.size() < 2) continue;
                auto &[target, role, idx] = key;

                /*bool all_time = std::all_of(entries.begin(), entries.end(),
                                            [](auto &e){ return e.first->trigger.kind == TRIG_TIME; });

                if (!all_time)
                {
                    issues.push_back({ target, role, idx,
                                       "conflicting automatic sources on same actuator: event-triggered mixed with другой источник" });
                    continue;
                }*/

                for (size_t i = 0; i < entries.size(); ++i)
                    for (size_t j = i + 1; j < entries.size(); ++j)
                    {
                        auto &ea = entries[i]; auto &eb = entries[j];
                        if (ea.second->activate == eb.second->activate)
                            continue; // одно направление — не противоречие, максимум избыточность

                        auto &ta = ea.first->trigger.time;
                        auto &tb = eb.first->trigger.time;

                        if (ta.period_sec != tb.period_sec)
                        {
                            issues.push_back({ target, role, idx,
                                               "time atoms with different periods on same actuator — overlap not provably excluded" });
                            continue;
                        }

                        float a0 = ta.offset_from_entry_sec, a1 = a0 + ta.duration_sec;
                        float b0 = tb.offset_from_entry_sec, b1 = b0 + tb.duration_sec;
                        bool disjoint = (a1 <= b0) || (b1 <= a0);

                        if (!disjoint)
                            issues.push_back({ target, role, idx,
                                               "overlapping time windows with opposite commands on same actuator" });
                    }
            }
        }
        return issues;
    }

    Instrument *CAutomationModel::add_internal_instrument(NComponent *owner, const SSignalRole &role)
    {
        for (auto *existing : m_instruments)
        {
            if (auto *in = dynamic_cast<CInternalInstrument *>(existing))
            {
                if (in->owner() == owner && in->role().role == role.role && in->role().index == role.index)
                {
                    return in;
                }
            }
        }

        auto *instr = new CInternalInstrument(owner, role);
        m_instruments.push_back(instr);
        return instr;
    }

    Actuator *CAutomationModel::add_internal_actuator(NComponent *owner, const SCommandRole &role)
    {
        // симметрично add_internal_instrument, через CInternalActuator
        for (auto *existing : m_actuators)
        {
            if (auto *actuator = dynamic_cast<Actuator *>(existing))
            {
                if (actuator->owner() == owner && actuator->role().role == role.role && actuator->role().index == role.index)
                {
                    return actuator;
                }
            }
        }

        auto *actuator = new Actuator(owner->input(0), role);//owner, role);
        m_actuators.push_back(actuator);
        return actuator;
    }

    void CAutomationModel::adopt_atom_set(SAtomSet &&set)
    {
        // id уникален по тем же правилам, что и add_atom_set — дубликат не создаём молча
        if (find_atom_set(set.id))
            return; // TODO: диагностика коллизии id, как и везде в этом файле
        m_atom_sets.push_back(std::move(set));
    }

    void CAutomationModel::add_internal_variable(const SVariableBehaviorSpec &spec)
    {
        m_variables.push_back(spec); // владелец уже проставлен вызывающим (spec.variable.owner)
    }

    void CAutomationModel::add_aggregate_signal(NComponent *owner, const SAggregateSignalSpec &spec)
    {
        m_aggregates.push_back({ owner, spec });
    }

    const std::vector<SVariableBehaviorSpec> &CAutomationModel::internal_variables() const { return m_variables; }
}
