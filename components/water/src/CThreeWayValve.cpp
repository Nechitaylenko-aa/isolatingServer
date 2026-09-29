#include "../include/CThreeWayValve.h"
#include "../../../include/Logger.h"
#include "../../calc/IShadowManager.h"

namespace NCore
{
    CThreeWayValve::CThreeWayValve(IGeneralTor *tor, CCell *owner)
            : NComponent(tor, owner, E_WATER_COMPONENTS::EWB_VALVE_WATER_THREE_WAY)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Three-way valve";
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

        /// Обязательно на все пины назначаем калбэк - когда пин соединяется/разъединяется он тыкает палкой "зацени" (меняется цвет).
        std::function<void()> callback = [this](){callback_from_pin(); };
        in->set_callbackOnConnect(callback);
        out->set_callbackOnConnect(callback);
        out1->set_callbackOnConnect(callback);

        CParameter valve_throughput(E_MEASURE_UNITS::EMU_CONSUMPTION, EMUCONS::emcs_liter_sec, 20, ESP_NONE, Tstring(""));

        m_parameters.push_back(valve_throughput);
    }

    CThreeWayValve::~CThreeWayValve() = default;

    bool CThreeWayValve::set_parameters(CContainer &container)
    {
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_outputs.at(1)->set_id(id_out1);
        return true;
    }

    void CThreeWayValve::get_parameters(CContainer &container)
    {
        container.add_member(m_id, "ID");
        id_in = m_inputs.at(0)->get_id();
        id_out = m_outputs.at(0)->get_id();
        id_out1 = m_outputs.at(1)->get_id();
        container.add_member(id_in, "id_input");
        container.add_member(id_out, "id_out");
        container.add_member(id_out1, "id_out1");
    }

    Tstring CThreeWayValve::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CThreeWayValve::put_ob(COperatingBody *body, CCap *sender)
    {
        *m_body = *body;

        if (sender == m_inputs.front())
        {
            calculateInBody();

            if (!m_is_open)
            {
                m_body->set_si_volume(0);
                return nullptr;
            }

            // Маршрутизация по текущей позиции — её решает automation-слой (set_position),
            // не тень: тень только подбирает DN/Kv/PN один раз, см. .h.
            CCap *active_output = (m_position == EValvePosition::A_TO_B)
                                   ? m_outputs.at(0) : m_outputs.at(1);
            return active_output->put_ob(m_body, nullptr);
        }

        if (sender == m_outputs.front() || sender == m_outputs.at(1))
            return m_inputs.front()->put_ob(m_body, nullptr);

        return nullptr;
    }

    void CThreeWayValve::calculateInBody()
    {
        Logger &logger = Logger::instance();

        if (m_equipProxy.equip_id == 0)
        {
            SEquipmentRequest request = IShadowManager::getEquipRequest(this, m_generalTor, m_body);
            if (!request.params.empty())
            {
                m_info_bus->addRequest(std::move(request));
            }
            else
            {
                logger.info("CThreeWayValve: no parameters to request equipment");
            }
            return;
        }
        // Кv/DN уже подобраны, второй расчёт не нужен — клапан не меняет тело содержательно.
    }

    void CThreeWayValve::set_equipment(equip::CEquipment *equip) {}
    void CThreeWayValve::set_equipmentProxy(std::vector<SComponentProxy> &&items)
    {
        if (items.empty())
            return;

        m_equipmentChoice = std::move(items);
        m_equipProxy = m_equipmentChoice.at(0);
    }

    std::vector<SSignalRole> CThreeWayValve::required_signals() const
    {
        return {}; // ничего не измеряет само по себе
    }

    std::vector<SCommandRole> CThreeWayValve::required_commands() const
    {
        return {
                { CR_FLOW_DIRECTION_VALVE, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SAutomationSpec> CThreeWayValve::automation() const
    {
        return {}; // сам по себе не решает, когда переключаться — управляется backwash-рецептом
    }

    std::vector<SDesignConstraintSpec> CThreeWayValve::design_constraints() const
    {
        return {};
    }
}
