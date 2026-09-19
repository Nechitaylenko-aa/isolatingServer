#include "CSimulationRunner.h"
#include "CSimGraphManager.h"

#include "../CAutomationModel.h"
#include "../actuator/Instrument.h"
#include "../actuator/Actuator.h"
#include "../IInstallationTemplate.h"
#include "NComponent.h"
#include "../actuator/CServiceInstrument.h"
#include "../../../server/sources/logger-common/logger.h"
#include <iostream>
#include "CSubProject.h"
#include "CSimShadowsManager.h"
#include "shadows/include/CSimPumpStation.h"
#include "shadows/include/CSimWaterCapacity.h"
#include "shadows/include/CSimValveTwoDirectional.h"
#include "shadows/include/CSimValveCut.h"


namespace NCore
{
    CSimulationRunner::CSimulationRunner(CAutomationModel *model) : m_model(model)
    {
        m_model->set_callback_on_bind([this]{ rebuild(); });
        rebuild();
    }

    CSimulationRunner::~CSimulationRunner()
    {
        m_model->set_callback_on_bind(nullptr); // раннер может быть уничтожен раньше модели
        for (auto &item : m_simShadowsMap)
        {
            delete item.second;
        }
        delete m_graphManager;
    }

    void CSimulationRunner::rebuild()
    {
        m_instrument_index.clear();
        m_actuator_index.clear();
        m_service_instruments.clear(); // владеющее хранилище — CServiceInstrument создаются раннером, не моделью

        /*for (auto &item : m_simShadowsMap)
        {
            delete item.second;
        }
        m_simShadowsMap.clear();*/

        for (auto *instr : m_model->instruments())
        {
            NComponent *owner = instr->owner();
            if (!owner) continue;
            m_instrument_index[{owner, instr->role().role, instr->role().index}] = instr;
        }

        for (auto *act : m_model->actuators())
        {
            m_actuator_index[{act->owner(), act->role().role, act->role().index}] = act;
        }

        // --- internal variables: живое хранилище + регистрация как читаемый Instrument ---
        auto &specs = m_model->internal_variables();
        m_variable_values.assign(specs.size(), 0.f);

        for (size_t i = 0; i < specs.size(); ++i)
        {
            m_variable_values[i] = specs[i].variable.initial_value;

            float *slot = &m_variable_values[i]; // адрес стабилен: m_variable_values не ресайзится после этого цикла
            auto instr = std::make_unique<CServiceInstrument>(
                    SSignalRole{ specs[i].exposed_role, specs[i].exposed_index, EMU_TIME, {}, ESP_NONE },
                    [slot]{ return *slot; });

            m_instrument_index[{ specs[i].variable.owner, specs[i].exposed_role, specs[i].exposed_index }] = instr.get();
            m_service_instruments.push_back(std::move(instr));
        }

        auto *subproject = m_model->installation_template()->sub_project();

        if (!m_graphManager)
            m_graphManager = new CSimGraphManager(m_model->installation_template()->sub_project(), &m_simShadowsMap);

        if (subproject && m_simShadowsMap.empty())
        {
            for (auto & item : *(subproject->project_cell()->get_components()))
            {
                auto *component = dynamic_cast<NComponent*>(item);
                if (component)
                {
                    auto shadow = CSimShadowsManager::instance(component);
                    if (shadow)
                    {
                        shadow->set_topology_changed_callback([&](){this->m_graphManager->rebuild_base();});
                        m_simShadowsMap.emplace(component->get_id(), shadow);
                    }
                }

            }
        }

        if (m_simShadowsMap.empty())
            CLogger::instance().error("CSimulationRunner::rebuild", "No shadows found! {}");


        //delete m_graphManager;
        m_graphManager->rebuild_base();

        m_active_set_id    = m_model->active_set_id();


        std::cout << "SET_NORMAL atoms count: " << m_model->atom_sets().at(0).atoms.size() << std::endl;

        //m_time_since_entry = 0.f;
        //m_t = 0.f;
        //m_prev_supervisory_results.clear();
        //m_prev_active_set_results.clear();
        //m_grains.clear();
    }

    float CSimulationRunner::read_signal(NComponent *owner, ESignalRole role, uint16_t index) const
    {
        auto it = m_instrument_index.find({owner, role, index});
        if (it == m_instrument_index.end())
        {
            CLogger::instance().error("CSimulationRunner::read_signal",
                                      "Несуществующий инструмент для владельца:{} {}; сигнал: {}, индекс: {}", owner->schematicName(), owner->get_id(),
                                      signal_roles_str[role], index);
            return -1.f;
        }

        //return owner->read_state_signal({role}, index);
        return it->second->value();
    }

    float CSimulationRunner::resolve_threshold(const SCondition &c) const
    {
        switch (c.threshold_source)
        {
            case ESS_FIXED:
                return c.fixed_threshold;
            case ESS_SIGNAL:
                return read_signal(c.compare_owner, c.compare_role, c.compare_index);
            case ESS_TOR_OUT_PRESSURE:
            case ESS_TOR_OUT_TURBIDITY:
                return m_model->installation_template()->resolve_tor_threshold(c.threshold_source, c);
        }
        return 0.f;
    }

    bool CSimulationRunner::evaluate_condition(const SCondition &c) const
    {
        float lhs = read_signal(c.signal_owner, c.role, c.index);
        float rhs = resolve_threshold(c);

        switch (c.op)
        {
            case CMP_LT: return lhs <  rhs;
            case CMP_LE: return lhs <= rhs;
            case CMP_GT: return lhs >  rhs;
            case CMP_GE: return lhs >= rhs;
            case CMP_EQ: return lhs == rhs;
            case CMP_NE: return lhs != rhs;
        }
        return false;
    }

    bool CSimulationRunner::evaluate_trigger(const STrigger &trig) const
    {
        switch (trig.kind)
        {
            case TRIG_NO_CONDITIONS:
                return true;
            case TRIG_TIME:
                return CAutomationModel::evaluate_time(trig.time, m_time_since_entry);
            case TRIG_EVENT:
                for (auto &c : trig.conditions)      // все AND
                    if (!evaluate_condition(c))
                        return false;
                return true;
        }
        return false;
    }

    void CSimulationRunner::advance_variables()
    {
        auto &specs = m_model->internal_variables();
        for (size_t i = 0; i < specs.size(); ++i)
        {
            const auto &spec = specs[i];
            if (spec.behavior == VB_ACCUMULATE_WHILE_TRUE)
            {
                bool active = evaluate_condition(SCondition{
                        spec.variable.owner, spec.condition_role, spec.condition_index,
                        CMP_EQ, ESS_FIXED, 1.0f });

                if (active)
                    m_variable_values[i] = std::min(m_variable_values[i] + m_granularity / 3600.f, spec.max_value);
            }
            else if (spec.behavior == VB_ACCUMULATE_RATE)
            {
                // condition_role здесь — НЕ условие, а источник мгновенной скорости
                // (напр. SR_FLOW_RATE узла учёта). Читаем как обычный сигнал, не как bool.
                float rate = read_signal(spec.variable.owner, spec.condition_role, spec.condition_index);
                m_variable_values[i] = std::min(m_variable_values[i] + rate * m_granularity, spec.max_value);
            }

            bool active = evaluate_condition(SCondition{
                    spec.variable.owner, spec.condition_role, spec.condition_index,
                    CMP_EQ, ESS_FIXED, 1.0f });

            if (active)
                m_variable_values[i] = std::min(m_variable_values[i] + m_granularity / 3600.f, spec.max_value);

            // has_reset обрабатывается в apply_single_command при команде reset_role —
            // раннер не проверяет reset здесь, это разовое событие по команде, не по тику
        }
    }

    void CSimulationRunner::apply_single_command(const SAtomCommand &cmd)
    {
        // сброс переменной по команде — проверяем раньше обычной actuator-логики. Фактически на будущее
        const std::vector<SVariableBehaviorSpec> & specs = m_model->internal_variables();
        for (size_t i = 0; i < specs.size(); ++i)
        {
            auto &spec = specs[i];
            if (spec.has_reset && spec.variable.owner == cmd.target_owner
                && spec.reset_role == cmd.command.role && spec.reset_index == cmd.command.index
                && cmd.activate)
            {
                m_variable_values[i] = 0.f;
            }
        }

        // Судя по всему аппендикс, или недоделка - остается в актуаторе как лакмусова бумажка
        auto it = m_actuator_index.find({cmd.target_owner, cmd.command.role, cmd.command.index});
        if (it == m_actuator_index.end())
        {
            CLogger::instance().error("CSimulationRunner::apply_single_command",
                                      "Actuator not found for {} id:{}",
                                      cmd.target_owner->get_description(), cmd.target_owner->get_id());
            return;
        }

        it->second->set_live_state(cmd.activate);
    }

    void CSimulationRunner::apply_actions(const std::vector<SAtomCommand> &actions)
    {
        for (auto &a : actions)
        {
            // apply_single_command(a);
            if (a.target_owner->get_id() == 256)
            {
                int t = 0;
            }
            process_command(a);
            m_grains.push_back({ m_t, describe_command(a) });
        }
    }

    bool CSimulationRunner::evaluate_and_diff()
    {
        bool any_diff = false;

        // снимок обязательного набора атомов
        std::vector<uint8_t> supervisory_now;
        supervisory_now.reserve(m_model->supervisory_atoms().size());

        for (auto &atom : m_model->supervisory_atoms())
        {
            bool r = evaluate_trigger(atom.trigger);
            supervisory_now.push_back(r);
        }

        // если сработал атом (это фронт), мы вызываем действие
        if (supervisory_now != m_prev_supervisory_results)
        {
            any_diff = true;
            for (size_t i = 0; i < supervisory_now.size(); ++i)
            {
                if (i >= m_prev_supervisory_results.size() ||
                    supervisory_now[i] != m_prev_supervisory_results[i])
                {
                    if (supervisory_now[i]) // применяем действия только сработавших, в порядке объявления
                    {
                        apply_actions(m_model->supervisory_atoms()[i].actions);
                    }
                }
            }
        }
        m_prev_supervisory_results = supervisory_now;

        // --- активный atom set: атомы + переходы --------------------------------------
        const SAtomSet *active = nullptr;
        for (auto &s : m_model->atom_sets())
        {
            if (s.id == m_active_set_id)
            {
                active = &s;
                break;
            }
        }
        if (!active) return any_diff;


        std::vector<uint8_t> active_now;
        active_now.reserve(active->atoms.size() + active->transitions.size());

        for (auto &atom : active->atoms)
        {
            active_now.push_back(evaluate_trigger(atom.trigger));
        }


        for (auto &tr : active->transitions)
        {
            active_now.push_back(evaluate_trigger(tr.trigger));
        }

        if (active_now != m_prev_active_set_results)
        {
            any_diff = true;
            size_t idx = 0;
            for (auto &atom : active->atoms)
            {
                if (active_now[idx] &&
                    (idx >= m_prev_active_set_results.size() || !m_prev_active_set_results[idx]))
                {
                    apply_actions(atom.actions);
                }
                ++idx;
            }
            for (auto &tr : active->transitions)
            {
                if (active_now[idx] &&
                    (idx >= m_prev_active_set_results.size() || !m_prev_active_set_results[idx]))
                {
                    const SAtomSet *to = find_set(tr.to_set_id);
                    m_grains.push_back({ m_t, to ? describe_transition(*active, *to)
                                                 : ("Переход в набор id=" + std::to_string(tr.to_set_id)) });
                    m_active_set_id    = tr.to_set_id;
                    m_time_since_entry = 0.f;
                    m_prev_active_set_results.clear();
                    return true;
                }
                ++idx;
            }
        }
        m_prev_active_set_results = active_now;
        return any_diff;
    }

    bool CSimulationRunner::step()
    {
        advance_variables();
        bool diff = evaluate_and_diff();
        if (diff)
            m_grains.push_back({m_t, "condition diff"}); // детализация — доработать при полном CSimulationTrace
        tick_all_shadows();
        m_time_since_entry += m_granularity;
        m_t += m_granularity;
        return diff;
    }

    Tstring CSimulationRunner::describe_command(const SAtomCommand &cmd)
    {
        Tstring who = cmd.target_owner->schematicName()
                      + " (id:" + std::to_string(cmd.target_owner->get_id()) + ")";

        switch (cmd.command.role)
        {
            case CR_PUMP_START_STOP:
            case CR_BLOWER_START_STOP:
                return who + (cmd.activate ? " — запущен" : " — остановлен");
            case CR_VALVE_OPEN_CLOSE:
                return who + (cmd.activate ? " — поток открыт" : " — поток закрыт");
            case CR_FLOW_DIRECTION_VALVE:
                return who + (cmd.activate ? " — направление потока ОП" : " — направление потока НОРМ");
            case CR_PUMP_FAULT_RESET:
                return who + " — сброс аварии";
            default:
                return who + " — " + command_roles_str[cmd.command.role]
                       + (cmd.activate ? " ON" : " OFF");
        }
    }

    Tstring CSimulationRunner::describe_transition(const SAtomSet &from, const SAtomSet &to)
    {
        return "Переход режима: " + from.display_name + " -> " + to.display_name;
    }

    const SAtomSet* CSimulationRunner::find_set(uint16_t id) const
    {
        for (auto &s : m_model->atom_sets())
            if (s.id == id) return &s;
        return nullptr;
    }

    void CSimulationRunner::print_grains() const
    {
        for (auto &g : m_grains)
            std::cout << "[t=" << g.time_sec << "s] " << g.description << std::endl;
    }

    void CSimulationRunner::run_and_print(float duration_sec)
    {
        CLogger::instance().init("", ELogLevel::LL_DEBUG);
        size_t printed = 0;
        while (m_t < duration_sec)
        {
            step();
            for (; printed < m_grains.size(); ++printed)
            {
                std::cout << "[t=" << m_grains[printed].time_sec << "s] "
                          << m_grains[printed].description << std::endl;
            }
        }
        CLogger::instance().shutdown();
    }

    void CSimulationRunner::process_command(const SAtomCommand &command)
    {

        auto sh_it = m_simShadowsMap.find(command.target_owner->get_id());
        ISimulationShadow *shadow = (sh_it != m_simShadowsMap.end()) ? sh_it->second : nullptr;
        if (!shadow || !m_graphManager)
            return;

        if (shadow->owner()->get_id() == 1156 || shadow->owner()->get_id() == 1153)
        {
            int k = 2;
        }

        switch (command.command.role)
        {
            case CR_PUMP_START_STOP:
                {
                    // По сути это самые уродливые костыли - мы приводим к конкретному водному классу в общем раннере
                    auto *capacityBefore = m_graphManager->find_nearest<CSimWaterCapacity>(command.target_owner, false);
                    auto *capacityAfter  = m_graphManager->find_nearest<CSimWaterCapacity>(command.target_owner,  true);

                    if (shadow->owner()->schematicName() == "НС8" && !command.activate)
                    {
                        CSimWaterCapacity *e11 = nullptr;
                        auto item = m_simShadowsMap.find(11);
                        if (item != m_simShadowsMap.end())
                        {
                            e11 = dynamic_cast<CSimWaterCapacity*>(item->second);
                        }

                        int k = 2;
                    }

                    if (capacityBefore)
                        capacityBefore->setPumpPost(command.activate ? shadow : nullptr);
                    if (capacityAfter)
                        capacityAfter->setPumpBefore(command.activate ? shadow : nullptr);
                }
                break;
            case CR_PUMP_FAULT_RESET:
                return;
            case CR_VALVE_OPEN_CLOSE:
                {
                    // По сути это самые уродливые костыли - мы приводим к конкретному водному классу в общем раннере
                    // TODO: Самый уродливый кусок раннера
                    if (shadow->owner()->get_subtype() == (TComponentType)E_WATER_COMPONENTS::EWB_VALVE_CUT)
                    {
                        auto *capacityAfter  = m_graphManager->find_nearest<CSimWaterCapacity>(command.target_owner,  true);

                        if (capacityAfter)
                            capacityAfter->setPumpBefore(command.activate ? shadow : nullptr);
                    }
                }
                break;
            case CR_FLOW_DIRECTION_VALVE:
                break;
            default:
                return;
        }
        shadow->apply_command(command.command, command.activate);
    }

    void CSimulationRunner::tick_all_shadows()
    {
        for (auto &[id, shadow] : m_simShadowsMap)
        {
            shadow->tick(m_t);
            NComponent *owner = shadow->owner();
            if (!owner) continue;
            for (auto &role : shadow->readable_state_roles())
            {
                owner->set_state_signal(role, role.index, shadow->read(role, role.index));
            }
        }
    }
}