//
// Created by artem on 28.05.26.
// Реализация к уже существующему CWaterCapacity.h — по образцу CLightFilter.cpp.
//
// Ёмкость — пассивный компонент (required_commands()/automation() пусты, см.
// automation_layer_context.md: "пусто у пассивных, вроде ёмкости"). Её единственная
// роль в слое автоматизации — источник SR_LEVEL_HIGH/SR_LEVEL_LOW, который
// IInstallationTemplate::wire_boundary_patterns находит и связывает с актуатором
// вверх по потоку (насос/задвижка) — сама ёмкость никогда не решает, кого остановить.
//
// put_ob — тождественное преобразование (ёмкость не меняет состав РТ). Физическое
// накопление объёма/уровня во времени — забота CSimulationShadow, которого ещё нет
// (следующий шаг после кандидатов интерлоков) — здесь только push-модель одного прохода.
//

#include "../include/CWaterCapacity.h"
#include "../../../include/Logger.h"
#include <cassert>

namespace NCore
{
    CWaterCapacity::CWaterCapacity(IGeneralTor *tor, CCell *owner)
        : NComponent(tor, owner, E_WATER_COMPONENTS::EWB_CAPACITY)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Water capacity";
        m_schName = "Е" + std::to_string(m_id);

        m_imgSource = ":/palette/images/palette/1.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        auto [in1, out1, body1] = add_ob(&ob, CT_ADDITIONAL);
        auto [in2, out2, body2] = add_ob(&ob, CT_ADDITIONAL);
        m_body = body;
        m_inputs.push_back(in);
        m_inputs.push_back(in1);
        m_inputs.push_back(in2);

        m_outputs.push_back(out);
        delete this->remove_output(out1);
        delete this->remove_output(out2);



        m_volume = new CParameter(E_MEASURE_UNITS::EMU_VOLUME, EMUVOL::emv_liter,
                                   0, EStandardPrefix::ESP_NONE, Tstring ("объём"));
        m_width = new CParameter(E_MEASURE_UNITS::EMU_DISTANCE, EMUDIS::emd_meter, 1000, ESP_MILLI, Tstring ("width"));
        m_deepness = new CParameter(E_MEASURE_UNITS::EMU_DISTANCE, EMUDIS::emd_meter, 1000, ESP_MILLI, Tstring ("deepness"));
        m_height = new CParameter(E_MEASURE_UNITS::EMU_DISTANCE, EMUDIS::emd_meter, 1000, ESP_MILLI, Tstring("Height"));
        m_volume->set_si_value(m_width->si_value() * m_deepness->si_value() * m_height->si_value());


        m_parameters.push_back(*m_volume);
    }

    CWaterCapacity::~CWaterCapacity()
    {
        delete m_volume;
        delete m_deepness;
        delete m_height;
        delete m_width;
    }

    bool CWaterCapacity::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)NComponent::m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_inputs.at(1)->set_id(m_idIn1);
        m_inputs.at(2)->set_id(m_idIn2);

        m_schName = "Е" + std::to_string(m_id);

        return true;
    }

    void CWaterCapacity::get_parameters(CContainer &container)
    {
        container.add_member(*m_volume, "Объём ёмкости");
        container.add_member(m_id, "ID");
        m_ser_comp_type = ComponentConverter::get_int_TComponentType(m_successor_component_type);
        container.add_member(m_ser_comp_type, "Component type");
        m_ser_project_type = get_project_type();
        container.add_member(m_ser_project_type, "project_type");

        id_in = m_inputs.at(0)->get_id();
        m_idIn1 = m_inputs.at(1)->get_id();
        m_idIn2 = m_inputs.at(2)->get_id();
        id_out = m_outputs.at(0)->get_id();

        container.add_member(id_in, "id_input");
        container.add_member(id_out, "id_out");
        container.add_member(m_idIn1, "id_input1");
        container.add_member(m_idIn2, "id_input12");
        container.add_member(id_out, "id_out");
    }

    Tstring CWaterCapacity::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CWaterCapacity::put_ob(COperatingBody *body, CCap *sender)
    {
        *m_body = *body;

        if (sender == m_inputs.front())
        {
            return m_outputs.front()->put_ob(m_body, nullptr);
        }
        if (sender == m_outputs.front())
        {
            m_inputs.front()->put_ob(m_body, nullptr);
        }
        return nullptr;
    }

    // ---- Слой 2 автоматизации ---------------------------------------------------------

    std::vector<SSignalRole> CWaterCapacity::required_signals() const
    {
        // Дискретные пороговые события уровня — не параметр протекающего тела,
        // поэтому unit/subtype — тот же дискретный EMU_AMOUNT/EA_ENUM, что уже
        // используется для команд промывки у CLightFilter (единственный сегодня
        // способ выразить "включено/выключено" без отдельного CAmountMeasure).
        // CS_COMPONENT_SCOPED — уровень это свойство ёмкости целиком, не конкретного
        // Cap; якорь на output(0) чисто физический (см. kStateDerivedSignalRoles —
        // обе роли ниже требуют CSimulationShadow, реально их value() пока не читается).
        return {
                {SR_LEVEL_HH, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM, EStandardPrefix::ESP_NONE,
                  CS_COMPONENT_SCOPED, false, 0 },
                {SR_LEVEL_LL, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM, EStandardPrefix::ESP_NONE,
                  CS_COMPONENT_SCOPED, false, 0 },
                {SR_LEVEL_UPPER, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM, EStandardPrefix::ESP_NONE,
                        CS_COMPONENT_SCOPED, false, 0 },
                {SR_LEVEL_MID, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM, EStandardPrefix::ESP_NONE,
                        CS_COMPONENT_SCOPED, false, 0 },
                {SR_LEVEL_BOTT, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM, EStandardPrefix::ESP_NONE,
                        CS_COMPONENT_SCOPED, false, 0 }

        };
    }

    std::vector<SCommandRole> CWaterCapacity::required_commands() const
    {
        // Пассивный компонент — сама ёмкость ничем не управляет. Кого остановить при
        // SR_LEVEL_HIGH решает не она, а общий проход wire_boundary_patterns.
        return {};
    }

    std::vector<SAutomationSpec> CWaterCapacity::automation() const
    {
        // Пассивный компонент — нет собственной динамики алгоритма.
        return {};
    }

    std::vector<SDesignConstraintSpec> CWaterCapacity::design_constraints() const
    {
        return {
                { DC_FLOW_CAPACITY_VS_NEIGHBOR,
                  "паспортный объём должен быть согласован с расходом соседних насосов/линий" }
        };
    }

    /*void CWaterCapacity::set_signal(ESignalRole role)
    {
        m_signal_role = role;
    }*/

    /*std::vector<SAggregateSignalSpec> CWaterCapacity::aggregate_signals() const
    {
        SAggregateSignalSpec spec;
        SSignalRole br, sr, ur, lr, hr;

        br.role = SR_LEVEL_BOTT;
        sr.role = SR_LEVEL_MID;
        ur.role = SR_LEVEL_UPPER;
        lr.role = SR_LEVEL_LL;
        lr.role = SR_LEVEL_HH;

        spec.op = EAggregateOp::AGG_ANY;
        spec.result_role = {};
        spec.source_roles = {br, sr, ur, lr, hr};

        return {spec};
    }*/
}
