//
// UF-лампа — см. TODO в заголовке насчёт CR_LAMP_ON_OFF/SR_LAMP_STATUS.
//

#include "CUFLamp.h"
#include <cassert>

namespace NCore
{
    CUFLamp::CUFLamp(IGeneralTor *tor, CCell *owner)
        : NComponent(tor, owner, EWB_UF_LAMP)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "UF lamp";
        m_schName = "УФ" + std::to_string(m_id);
        m_imgSource = ":/palette/images/palette/22.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        m_inputs.push_back(in);
        m_outputs.push_back(out);
        m_body = body;
    }

    CUFLamp::~CUFLamp()
    = default;

    bool CUFLamp::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_schName = "УФ" + std::to_string(m_id);
        return true;
    }

    void CUFLamp::get_parameters(CContainer &container)
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

    Tstring CUFLamp::get_description() const
    {
        return m_descript;
    }

    // identity-эффект — лампа в этой модели не меняет измеримые параметры РТ,
    // только обеззараживает (не моделируется как параметр состава); по образцу
    // CGenericComponent::put_ob
    COperatingBody *CUFLamp::put_ob(COperatingBody *body, CCap *sender)
    {
        *m_body = *body;

        if (sender == m_inputs.front())
            return m_outputs.front()->put_ob(m_body, nullptr);
        if (sender == m_outputs.front())
            m_inputs.front()->put_ob(body, nullptr);
        return nullptr;
    }

    std::vector<SSignalRole> CUFLamp::required_signals() const
    {
        return {
                // TODO: SR_LAMP_STATUS ещё нет в каталоге — см. TODO в заголовке
                { SR_LAMP_STATUS, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  EStandardPrefix::ESP_NONE, CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SCommandRole> CUFLamp::required_commands() const
    {
        return {
                // TODO: CR_LAMP_ON_OFF ещё нет в каталоге — см. TODO в заголовке
                { CR_LAMP_ON_OFF, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SAutomationSpec> CUFLamp::automation() const
    {
        // Ручное вкл/выкл — никакого расписания/PID не объявляем. Если появится
        // требование "включать по факту потока" — добавить TT_CONDITION здесь.
        return {};
    }

    std::vector<SDesignConstraintSpec> CUFLamp::design_constraints() const
    {
        // Открытый вопрос "не включать без потока" — не решён, оставлено пустым
        return {};
    }
}
