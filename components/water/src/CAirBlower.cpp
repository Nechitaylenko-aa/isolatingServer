#include "CAirBlower.h"
#include "Logger.h"
#include "../../CGenericComponent.h"
#include "../../../automation/CTechnologyBuilder.h"
#include "../../../automation/codegen/CCodegenShadowsManager.h"
#include "../../../automation/codegen/shadows/include/CShadowAirBlower.h"
#include <cassert>

namespace NCore
{
    CAirBlower::CAirBlower(IGeneralTor *tor, CCell *owner, uint16_t blower_count)
        : NComponent(tor, owner, EWB_AIR_BLOWER)
        , m_blower_count(blower_count)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Air blower station";
        m_schName = "ВС-" + std::to_string(m_id);
        m_imgSource = ":/palette/images/palette/4.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        m_body = body;
        m_inputs.push_back(in);
        m_outputs.push_back(out);

        m_nominal_pressure = new CParameter(E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale,
                                             0, EStandardPrefix::ESP_NONE, "напор");
        m_nominal_pressure->set_value(300000); // TODO: паспортное значение по умолчанию

        m_parameters.push_back(*m_nominal_pressure);
    }

    CAirBlower::~CAirBlower()
    {
        delete m_nominal_pressure;
    }

    bool CAirBlower::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)NComponent::m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_schName = "ВС" + std::to_string(m_id);
        return true;
    }

    void CAirBlower::get_parameters(CContainer &container)
    {
        container.add_member(*m_nominal_pressure, "Напор станции");
        container.add_member(m_blower_count, "Количество воздуходувок");
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

    Tstring CAirBlower::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CAirBlower::put_ob(COperatingBody *body, CCap *sender)
    {
        // Заглушка гидравлики/пневматики (не цель этой сессии) — см. аналогичный
        // комментарий в CPumpStation::put_ob.
        *m_body = *body;
        m_schName = "ВС" + std::to_string(m_id);

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

    std::vector<SSignalRole> CAirBlower::required_signals() const
    {
        std::vector<SSignalRole> signals;
        signals.push_back({ SR_PRESSURE_PV, 0, E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale,
                             EStandardPrefix::ESP_NONE, CS_FLOW_SPECIFIC, false, 0 });

        signals.push_back({SR_PUMP_STATION_FAIL, 0, EMU_AMOUNT, EMUAMO::EA_ENUM, ESP_NONE, CS_COMPONENT_SCOPED, false, 0});
        return signals;
    }

    std::vector<SCommandRole> CAirBlower::required_commands() const
    {
        return {
                { CR_PUMP_START_STOP, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SAutomationSpec> CAirBlower::automation() const
    {
        auto commands = required_commands();
        SSignalRole sr{SR_PRESSURE_PV, 0, E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale,
                       EStandardPrefix::ESP_NONE, CS_FLOW_SPECIFIC, false, 0};
        return {
                { TT_CONTINUOUS_PV, EA_NONE, sr.role, 0, 0u, 0, SN_DUTY_CYCLE, commands, ESS_FIXED, 0.0f }
        };
    }

    std::vector<SDesignConstraintSpec> CAirBlower::design_constraints() const
    {
        return {
                { DC_FLOW_CAPACITY_VS_NEIGHBOR,
                  "суммарная производительность воздуходувок не должна превышать паспортную "
                  "потребность аэротенка/следующего по потоку оборудования" }
        };
    }

    void CAirBlower::rebuild_internal_topology()
    {
        /*if (m_blower_count == 1)
            return;*/

        CTechnologyBuilder::build_parallel_branches(
                this, m_blower_count,
                [&](CCell *owner) -> CCell*
                {
                    SCaps caps{ {CT_MAIN}, {CT_MAIN} };
                    return new CGenericComponent(m_generalTor, this, EWB_GENERIC_COMP, caps);
                });
    }

    std::vector<SVariableBehaviorSpec> CAirBlower::variable_behaviors() const
    {
        std::vector<SVariableBehaviorSpec> result;
        for (uint16_t i = 0; i < m_blower_count; ++i)
        {
            SVariableBehaviorSpec spec;
            spec.variable.name          = "moto_hours_" + std::to_string(i);
            spec.variable.type_variable = ETV_FLOAT;
            spec.variable.retain        = true;
            spec.variable.initial_value = 0;
            spec.behavior      = VB_ACCUMULATE_WHILE_TRUE;
            spec.condition_role  = SR_PUMP_STATUS;
            spec.condition_index = i;
            spec.has_reset = false;
            spec.max_value = 100000;
            spec.exposed_index = i;
            spec.exposed_role = SR_MOTO_HOURS;
            result.push_back(spec);
        }
        return result;
    }

    /*std::vector<SAggregateSignalSpec> CAirBlower::aggregate_signals() const
    {
        SAggregateSignalSpec agg;
        agg.result_role = required_signals().back();
        agg.op = AGG_ALL;
        for (uint16_t i = 0; i < m_blower_count; ++i)
        {
            SSignalRole src = agg.result_role;
            src.role = SR_PUMP_STATUS;
            src.index = i;
            agg.source_roles.push_back(src);
        }
        return { agg };
    }

    std::vector<SAutomationAtom> CAirBlower::build_internal_atoms(
            const std::vector<SBoundaryInterlockCandidate> &external_candidates) const
    {
        std::vector<SAutomationAtom> arr;
        auto comp = dynamic_cast<CAirBlower*>(m_owner);
        if (comp)
        {
            auto shadow = dynamic_cast<CShadowAirBlower*>(CCodegenShadowsManager::codegen_shadow_instance(comp));
            SRotationGroupSpec spec = shadow->build_rotation_spec();
            arr = shadow->build_rotation_atoms(spec, external_candidates);
        }
        return arr;
    }*/
}
