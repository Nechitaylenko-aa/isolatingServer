#include "../include/CAccountNode.h"
#include "../../../include/Logger.h"
#include <cassert>

namespace NCore
{
    CAccountNode::CAccountNode(IGeneralTor *tor, CCell *owner)
        : NComponent(tor, owner, E_WATER_COMPONENTS::EWB_ACCOUNT_NODE)
    {
        m_descript = "Account node";
        m_schName = "УУ" + std::to_string(m_id);
        m_imgSource = ":/palette/images/palette/23.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);

        m_inputs.push_back(in);
        m_outputs.push_back(out);
        m_body = body;

        m_parameter = new CParameter(E_MEASURE_UNITS::EMU_DISTANCE, EMUDIS::emd_meter, 120, ESP_MILLI, Tstring{});
    }

    CAccountNode::~CAccountNode()
    {
        delete m_parameter;
    }

    bool CAccountNode::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)NComponent::m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_schName = "УУ" + std::to_string(m_id);
        return true;
    }

    void CAccountNode::get_parameters(CContainer &container)
    {
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

    Tstring CAccountNode::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CAccountNode::put_ob(COperatingBody *body, CCap *sender)
    {
        // identity — узел учёта не меняет состав/объём РТ, только измеряет попутно
        *m_body = *body;

        if (sender == m_inputs.front())
            return m_outputs.front()->put_ob(m_body, nullptr);
        if (sender == m_outputs.front())
            m_inputs.front()->put_ob(body, nullptr);
        return nullptr;
    }

    void CAccountNode::set_equipment(equip::CEquipment *equip) {}

    void CAccountNode::set_equipmentProxy(std::vector<SEquipLight> &&items)
    {
        if (items.empty()) return;
        m_equipmentChoice = std::move(items);
        m_equipProxy = m_equipmentChoice.at(0);
    }

    // ---- Слой 2 автоматизации -------------------------------------------------------
    // output(0) — физический якорь, CS_FLOW_SPECIFIC: температура/давление/расход
    // ИМЕННО в этой точке трубопровода имеют значение (не "про компонент вообще",
    // как перепад давления фильтра, а привязаны к конкретному сечению).

    std::vector<SSignalRole> CAccountNode::required_signals() const
    {
        return {
                { SR_PRESSURE_PV,    0, E_MEASURE_UNITS::EMU_PRESSURE,    EMUPRE::emp_pascale, EStandardPrefix::ESP_NONE,
                  CS_FLOW_SPECIFIC, false, 0 },
                { SR_TEMPERATURE_PV, 0, E_MEASURE_UNITS::EMU_TEMPERATURE, EMUTEM::emt_celsius, EStandardPrefix::ESP_NONE,
                  CS_FLOW_SPECIFIC, false, 0 },
                { SR_FLOW_RATE,      0, E_MEASURE_UNITS::EMU_VOLUME,      EMUVOL::emv_liter,   EStandardPrefix::ESP_NONE,
                  CS_FLOW_SPECIFIC, false, 0 }  // state-derived — см. kStateDerivedSignalRoles
        };
    }

    std::vector<SCommandRole> CAccountNode::required_commands() const
    {
        return {}; // узел учёта ничем не управляет
    }

    std::vector<SAutomationSpec> CAccountNode::automation() const
    {
        return {}; // нет собственного алгоритма — только измерение
    }

    std::vector<SDesignConstraintSpec> CAccountNode::design_constraints() const
    {
        return {}; // TODO: паспортный диапазон измерения vs фактический расход соседей — не сейчас
    }

    std::vector<SVariableBehaviorSpec> CAccountNode::variable_behaviors() const
    {
        SVariableBehaviorSpec totalizer;
        totalizer.variable.name = "flow_total";
        totalizer.variable.type_variable = ETV_FLOAT;
        totalizer.variable.retain = true;
        totalizer.variable.initial_value = 0.f;

        totalizer.behavior = VB_ACCUMULATE_RATE; // см. патч CAutomationAtoms.h
        totalizer.condition_role  = SR_FLOW_RATE; // здесь переиспользуется как "источник скорости", не условие
        totalizer.condition_index = 0;
        totalizer.has_reset = false;
        totalizer.max_value = 1'000'000.f; // TODO: реалистичный предел тотализатора

        totalizer.exposed_role  = SR_FLOW_TOTAL;
        totalizer.exposed_index = 0;

        return { totalizer };
    }

    CParameter *CAccountNode::diameter() const
    {
        return m_parameter;
    }

    COperatingBody *CAccountNode::ob() const
    {
        return m_inputs.front()->get_ob();
    }
}
