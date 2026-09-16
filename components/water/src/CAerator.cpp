//
// CAerator — см. TODO/пояснения в заголовке. Пассивный проточный компонент.
//

#include "CAerator.h"
#include <cassert>

namespace NCore
{
    CAerator::CAerator(IGeneralTor *tor, CCell *owner)
        : NComponent(tor, owner, EWB_AERATOR)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Aerator";
        m_schName = "АЭ" + std::to_string(m_id);
        m_imgSource = ":/palette/images/palette/16.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        m_inputs.push_back(in);
        m_outputs.push_back(out);
        m_body = body;
    }

    CAerator::~CAerator()
    = default;

    bool CAerator::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_schName = "АЭ" + std::to_string(m_id);
        return true;
    }

    void CAerator::get_parameters(CContainer &container)
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

    Tstring CAerator::get_description() const
    {
        return m_descript;
    }

    // identity — эффект на РТ (перенос кислорода и т.п.) не смоделирован, см. заголовок
    COperatingBody *CAerator::put_ob(COperatingBody *body, CCap *sender)
    {
        *m_body = *body;

        if (sender == m_inputs.front())
            return m_outputs.front()->put_ob(m_body, nullptr);
        if (sender == m_outputs.front())
            m_inputs.front()->put_ob(body, nullptr);
        return nullptr;
    }

    std::vector<SSignalRole> CAerator::required_signals() const
    {
        // Пассивен: physical эффект не смоделирован, публиковать пока нечего.
        return {};
    }

    std::vector<SCommandRole> CAerator::required_commands() const
    {
        return {}; // нами не управляется
    }

    std::vector<SAutomationSpec> CAerator::automation() const
    {
        return {};
    }

    std::vector<SDesignConstraintSpec> CAerator::design_constraints() const
    {
        return {}; // не выдумываю проверку без уточнения физики
    }
}
