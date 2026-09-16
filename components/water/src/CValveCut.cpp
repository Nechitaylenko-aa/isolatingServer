//
// CValveCut — см. заголовок.
//

#include "CValveCut.h"
#include <cassert>

namespace NCore
{
    CValveCut::CValveCut(IGeneralTor *tor, CCell *owner)
        : NComponent(tor, owner, EWB_VALVE_CUT)
    {
        m_descript = "Cut valve";
        m_schName = "КО-" + std::to_string(m_id);
        m_imgSource = ":/palette/images/palette/24.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        m_inputs.push_back(in);
        m_outputs.push_back(out);
        m_body = body;
    }

    bool CValveCut::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        return true;
    }

    void CValveCut::get_parameters(CContainer &container)
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

    Tstring CValveCut::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CValveCut::put_ob(COperatingBody *body, CCap *sender)
    {
        // Design-time: identity-проход тела. Реальное перекрытие потока по факту
        // открыт/закрыт — забота симуляционного слоя (CSimValveCut + CSimGraphManager
        // actual-граф, mark_dirty()), не этого метода.
        *m_body = *body;

        if (sender == m_inputs.front())
            return m_outputs.front()->put_ob(m_body, nullptr);
        if (sender == m_outputs.front())
            m_inputs.front()->put_ob(body, nullptr);
        return nullptr;
    }

    std::vector<SSignalRole> CValveCut::required_signals() const
    {
        return {
                { SR_VALVE_STATUS, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 },
        };
    }

    std::vector<SCommandRole> CValveCut::required_commands() const
    {
        return {
                { CR_VALVE_OPEN_CLOSE, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  CS_COMPONENT_SCOPED, false, 0 },
        };
    }

    std::vector<SAutomationSpec> CValveCut::automation() const
    {
        // Пассивный: сам по себе ничего не решает — реагирует только на боундари-
        // паттерн (аварийные уровни и т.п.), собираемый общим проходом резолвера.
        return {};
    }

    std::vector<SDesignConstraintSpec> CValveCut::design_constraints() const
    {
        return {};
    }
}
