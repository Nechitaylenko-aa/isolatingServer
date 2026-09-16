
#include "../include/CPumpStation.h"
#include "../../../include/Logger.h"
#include "../../CGenericComponent.h"
#include "../../../automation/CTechnologyBuilder.h"
#include "../../../automation/codegen/CCodegenShadowsManager.h"
#include "../../../automation/codegen/shadows/include/CShadowPumpStation.h"
#include <cassert>


namespace NCore
{
    CPumpStation::CPumpStation(IGeneralTor *tor, CCell *owner, uint16_t pump_count)
        : NComponent(tor, owner, E_WATER_COMPONENTS::EWB_PUMP_STATION)
        , m_pump_count(pump_count)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Pump station";
        m_schName = "НС" + std::to_string(m_id);

        m_imgSource = ":/palette/images/palette/2.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        m_body = body;
        m_inputs.push_back(in);
        m_outputs.push_back(out);

        m_nominal_pressure = new CParameter(E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale,
                                             6, EStandardPrefix::ESP_MEGA, "напор");

        m_parameters.push_back(*m_nominal_pressure);
    }

    CPumpStation::~CPumpStation()
    {
        delete m_nominal_pressure;
    }

    bool CPumpStation::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)NComponent::m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);

        m_schName = "НС" + std::to_string(m_id);

        return true;
    }

    void CPumpStation::get_parameters(CContainer &container)
    {
        container.add_member(*m_nominal_pressure, "Напор станции");
        container.add_member(m_pump_count, "Количество насосов");
        container.add_member(m_id, "ID");
        m_ser_comp_type = ComponentConverter::get_int_TComponentType(m_successor_component_type);
        container.add_member(m_ser_comp_type, "Component type");
        m_ser_project_type = get_project_type();
        container.add_member(m_ser_project_type, "project_type");

        id_in = m_inputs.at(0)->get_id();
        id_out = m_outputs.at(0)->get_id();

        container.add_member(id_in, "id_input");
        container.add_member(id_out, "id_out");
    }

    Tstring CPumpStation::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CPumpStation::put_ob(COperatingBody *body, CCap *sender)
    {
        *m_body = *body;

        if (sender == m_inputs.front())
        {
            m_body->set_si_pressure(m_nominal_pressure->si_value());
            return m_outputs.front()->put_ob(m_body, nullptr);
        }
        if (sender == m_outputs.front())
        {
            m_inputs.front()->put_ob(m_body, nullptr);
        }
        return nullptr;
    }

    // ---- Слой 2 автоматизации ---------------------------------------------------------

    std::vector<SSignalRole> CPumpStation::required_signals() const
    {
        std::vector<SSignalRole> signals;

        // Давление на выходе станции — смысл специфичен именно выходной стороне
        signals.push_back({ SR_PRESSURE_PV, 0, E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale,
                             EStandardPrefix::ESP_NONE, CS_FLOW_SPECIFIC, false, 0 });
        // если все насосы в аварии тоже нужен сигнал
        signals.push_back({SR_PUMP_STATION_FAIL, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, ESP_NONE,
                           CS_COMPONENT_SCOPED, false, 0}); //CR_DEFAULT_ACTIVATE

        return signals;
    }

    std::vector<SCommandRole> CPumpStation::required_commands() const
    {
        std::vector<SCommandRole> commands;

        SCommandRole cr{ CR_PUMP_START_STOP, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                         CS_COMPONENT_SCOPED, false, 0};

        commands.push_back(cr);
        return commands;
    }

    std::vector<SAutomationSpec> CPumpStation::automation() const
    {
        auto commands = required_commands();

        SSignalRole sr{SR_PRESSURE_PV, 0, E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale,
                       EStandardPrefix::ESP_NONE, CS_FLOW_SPECIFIC, false, 0};

        return {
                {
                    TT_CONTINUOUS_PV, EA_NONE, sr.role, 0, 0u,
                    0, SN_DUTY_CYCLE, commands,
                    ESS_TOR_OUT_PRESSURE, 0.0f
                }
        };
    }

    std::vector<SDesignConstraintSpec> CPumpStation::design_constraints() const
    {
        return {
                { DC_FLOW_CAPACITY_VS_NEIGHBOR,
                  "суммарная производительность насосов не должна превышать паспортную "
                  "пропускную способность следующего по потоку оборудования" }
        };
    }

    void CPumpStation::rebuild_internal_topology()
    {
        CTechnologyBuilder::build_parallel_branches(
                this, m_pump_count,
                [&](CCell *owner) -> CCell*
                {
                    SCaps caps{ {CT_MAIN}, {CT_MAIN} };
                    return new CGenericComponent(m_generalTor, this, EWB_GENERIC_COMP, caps);
                });
    }

    std::vector<SVariableBehaviorSpec> CPumpStation::variable_behaviors() const
    {
        std::vector<SVariableBehaviorSpec> result;
        for (uint16_t i = 0; i < m_pump_count; ++i)
        {
            SVariableBehaviorSpec spec;
            spec.variable.name          = "moto_hours_" + std::to_string(i);
            spec.variable.type_variable = ETV_FLOAT;
            spec.variable.retain        = true;         // физическая наработка — переживает рестарт
            spec.variable.initial_value = 0;
            spec.behavior      = VB_ACCUMULATE_WHILE_TRUE;
            spec.condition_role  = SR_PUMP_STATUS;
            spec.condition_index = i;
            spec.has_reset = false; // моточасы НЕ сбрасываются командой — только копятся
            spec.max_value = 100000;
            spec.exposed_index = i;
            spec.exposed_role = SR_MOTO_HOURS;
            result.push_back(spec);
        }
        return result;
    }

    /*std::vector<SAggregateSignalSpec> CPumpStation::aggregate_signals() const
    {
        SAggregateSignalSpec agg;
        agg.result_role = required_signals().back(); // SR_PUMP_STATION_FAIL, как уже объявлено
        agg.op = AGG_ALL;
        for (uint16_t i = 0; i < m_pump_count; ++i)
        {
            SSignalRole src = agg.result_role;
            src.role = SR_PUMP_STATUS;
            src.index = i;
            agg.source_roles.push_back(src);
        }
        return { agg };
    }

    std::vector<SAutomationAtom> CPumpStation::build_internal_atoms(
            const std::vector<SBoundaryInterlockCandidate> &external_candidates) const
    {
        std::vector<SAutomationAtom> arr;
        auto comp = dynamic_cast<CPumpStation*>(m_owner);
        if (comp)
        {
            auto shadow = dynamic_cast<CShadowPumpStation*>(CCodegenShadowsManager::codegen_shadow_instance(comp));
            SRotationGroupSpec spec = shadow->build_rotation_spec();//, external_candidates
            arr = shadow->build_rotation_atoms(spec, external_candidates);
        }

        return arr;
    }*/

}
