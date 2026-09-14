//
// Created by artem on 30.10.25.
//

#include "../include/CLightFilter.h"
#include "../../../equipment/water/include/CEquipmentLightFilterProxy.h"
#include "../../../include//Logger.h"
#include <cassert>
#include "../../calc/IShadowManager.h"

namespace NCore
{
    CLightFilter::CLightFilter(IGeneralTor *tor, CCell *owner) : NComponent(tor, owner, E_WATER_COMPONENTS::EWB_WATER_FILTER_LIGHT)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Filter light";
        m_schName = "ФО" + std::to_string(m_id);

        /// для эмблемы компонента
        m_imgSource = ":/palette/images/palette/17.png";

        /// Обязательно создаём входные и выходные пины. Они и будут соединяться в сеть.
        COperatingBody ob(BT_WATER);
        auto [in, out,body] = add_ob(&ob);
        auto [in1, out1,body1] = add_ob(&ob, CT_ADDITIONAL);

        m_inputs.push_back(in);
        m_outputs.push_back(out);

        m_inputs.push_back(in1);
        m_outputs.push_back(out1);

        m_body = body;

        /// Параметры устройства по умолчанию. В дальнейшем они должны браться из оборудования.
        VSubtypes sType;
        m_throughput_capacity = new CParameter(E_MEASURE_UNITS::EMU_VOLUME, EMUVOL::emv_liter,
                                               0, EStandardPrefix::ESP_NONE, "загруз.");
        m_throughput_capacity->set_value(20);

        m_average_filtration = new CParameter(E_MEASURE_UNITS::EMU_VOLUME, EMUVOL::emv_liter,
                                              0, EStandardPrefix::ESP_MILLI, "фильтрация");
        m_average_filtration->set_value(150);

        /// Наполнить параметрами для отображения в свойствах. Эти параметры передаст в GUI визуальный враппер
        m_parameters.push_back(*m_throughput_capacity);
        m_parameters.push_back(*m_average_filtration);
    }

    CLightFilter::~CLightFilter()
    {
        delete m_throughput_capacity;
        delete m_average_filtration;
    }

    bool CLightFilter::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)NComponent::m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_inputs.at(1)->set_id(id_in1);
        m_outputs.at(0)->set_id(id_out);
        m_outputs.at(1)->set_id(id_out1);


        m_schName = "ФО" + std::to_string(m_id);

        return true;
    }

    void CLightFilter::get_parameters(CContainer &container)
    {
        // TODO: Задействовать мульти язычество
        container.add_member(*m_average_filtration, "Среднее фильтрование");
        container.add_member(*m_throughput_capacity, "Пропускная способность");
        container.add_member(m_id, "ID");
        m_ser_comp_type = ComponentConverter::get_int_TComponentType(m_successor_component_type);
        container.add_member(m_ser_comp_type, "Component type");
        m_ser_project_type = get_project_type();
        container.add_member(m_ser_project_type, "project_type");

        id_in = m_inputs.at(0)->get_id();
        id_out = m_outputs.at(0)->get_id();
        id_in1 = m_inputs.at(1)->get_id();
        id_out1 = m_outputs.at(1)->get_id();

        container.add_member(id_in, "id_input");
        container.add_member(id_out, "id_out");
        container.add_member(id_in1, "id_input1");
        container.add_member(id_out1, "id_out1");
    }

    Tstring CLightFilter::get_description() const
    {
        // TODO: Задействовать мульти язычество
        return m_descript;
    }

    /** @brief единственны метод для всех двух-пиновых компонентов для работы над телом*/
    COperatingBody *CLightFilter::put_ob(COperatingBody *body, CCap *sender)
    {
        *m_body = *body;

        if (sender == m_inputs.front())
        {
            calculateInBody();
            return m_outputs.front()->put_ob(m_body, nullptr);
        }
        if (sender == m_outputs.front())
        {
            m_inputs.front()->put_ob(body, nullptr);
        }
        return nullptr;
    }


    void CLightFilter::calculateInBody()
    {
        Logger &logger = Logger::instance();
        CParameter* turbidity_param = nullptr;

        for (uint32_t i = 0; i < m_body->parameters_count(); ++i)
        {
            auto param = m_body->get_parameter(i);
            Tstring pName = param->unit_name();
            if (pName == mutnost)
            {
                turbidity_param = param;
                break;
            }
        }

        if (!turbidity_param)
        {
            logger.error("CLightFilter: There is no parameter '" + mutnost + "' in operating body");
            return;
        }

        if (m_equipProxy.equip_id == 0)
        {
            SEquipmentRequest request = IShadowManager::getEquipRequest(this, m_generalTor, m_body);
            if (!request.params.empty())
            {
                m_info_bus->addRequest(std::move(request));
            }
            else
            {
                logger.error("CLightFilter: no parameters to request equipment");
            }
            return;
        }

        auto values = IShadowManager::getBodyParams(this, m_equipProxy, m_generalTor, m_body);

        if (!values.empty())
        {
            turbidity_param->set_si_value(values.front());
        }
        else
        {
            logger.error("CLightFilter: no calculations result with equipment");
        }
    }


    void CLightFilter::set_equipment(equip::CEquipment *equip)
    {

    }

    void CLightFilter::set_equipmentProxy(std::vector<SEquipLight> &&items)
    {
        if (items.empty())
            return;

        m_equipmentChoice = std::move(items);
        m_equipProxy = m_equipmentChoice.at(0);
    }

    // ---- Слой 2 автоматизации ------------------------------------------------------------
    // См. automation_layer_summary.md — cap_index/is_input компонент выбирает детерминированно,
    // не "любой доступный". У фильтра один вход/один выход, поэтому оба хука ниже используют
    // output(0) как физический якорь для CS_COMPONENT_SCOPED-ролей (перепад давления и команды
    // промывки — свойства компонента целиком, не конкретного наконечника потока).

    std::vector<SSignalRole> CLightFilter::required_signals() const
    {
        return {
                { SR_DP_FILTER, 0, E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale, EStandardPrefix::ESP_NONE,
                  CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SCommandRole> CLightFilter::required_commands() const
    {
        return {
                { CR_FLOW_DIRECTION_VALVE, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                        CS_COMPONENT_SCOPED, false, 0 },
                { CR_FLOW_DIRECTION_VALVE, 1, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                        CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SAutomationSpec> CLightFilter::automation() const
    {
        auto commands = required_commands();
        return {
                { TT_SCHEDULE, EA_NONE, ESignalRole{}, 0, /*period_seconds*/ 3u * 24u * 3600u, 0, SN_PROCEDURE,
                  commands, ESS_FIXED, 0.0f }
        };
    }

    std::vector<SDesignConstraintSpec> CLightFilter::design_constraints() const
    {
        // формализация уже существующей проверки из CShadowLightFilter::getCalculationsWithEquip —
        // не новый механизм, а перенос её в объявляемую форму
        return {
                { DC_FILTRATION_VELOCITY_VS_PRESSURE,
                  "скорость фильтрации по факт. давлению не должна превышать паспортную v_max" }
        };
    }

}
