#include "../include/CTwoWayValve.h"

namespace NCore
{
    CTwoWayValve::CTwoWayValve(IGeneralTor *tor, CCell *owner)
            : NComponent(tor, owner, E_WATER_COMPONENTS::EWB_VALVE_WATER_TWO_WAY)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Two-way valve";
        m_schName = "КО" + std::to_string(m_id);
        m_imgSource = ":/palette/images/palette/25.png";

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        auto [in1, out1, body1] = add_ob(&ob);
        m_body = body;
        m_inputs.push_back(in);
        m_outputs.push_back(out);
        m_outputs.push_back(out1);

        delete this->remove_input(in1);

        CParameter valve_throughput(E_MEASURE_UNITS::EMU_CONSUMPTION, EMUCONS::emcs_liter_sec, 20);

        m_parameters.push_back(valve_throughput);
    }

    CTwoWayValve::~CTwoWayValve() = default;

    bool CTwoWayValve::set_parameters(CContainer &container)
    {
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_outputs.at(1)->set_id(id_out1);
        return true;
    }

    void CTwoWayValve::get_parameters(CContainer &container)
    {
        container.add_member(m_id, "ID");
        id_in = m_inputs.at(0)->get_id();
        id_out = m_outputs.at(0)->get_id();
        id_out1 = m_outputs.at(1)->get_id();
        container.add_member(id_in, "id_input");
        container.add_member(id_out, "id_out");
        container.add_member(id_out1, "id_out1");
    }

    Tstring CTwoWayValve::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CTwoWayValve::put_ob(COperatingBody *body, CCap *sender)
    {
        *m_body = *body;

        if (!m_is_open)
        {
            // закрытый патрубок — как ты и сказал, обнуляем объём, дальше не пускаем.
            m_body->set_si_volume(0);
            return nullptr;
        }

        if (sender == m_inputs.front())
            return m_outputs.front()->put_ob(m_body, nullptr);

        if (sender == m_outputs.front())
            return m_inputs.front()->put_ob(m_body, nullptr);

        return nullptr;
    }

    void CTwoWayValve::set_equipment(equip::CEquipment *equip) {}
    void CTwoWayValve::set_equipmentProxy(std::vector<SEquipLight> &&items) {}

    std::vector<SSignalRole> CTwoWayValve::required_signals() const
    {
        return {}; // ничего не измеряет само по себе
    }

    std::vector<SCommandRole> CTwoWayValve::required_commands() const
    {
        return {
                { CR_FLOW_DIRECTION_VALVE, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SAutomationSpec> CTwoWayValve::automation() const
    {
        return {}; // сам по себе не решает, когда переключаться — управляется backwash-рецептом
    }

    std::vector<SDesignConstraintSpec> CTwoWayValve::design_constraints() const
    {
        return {};
    }
}
