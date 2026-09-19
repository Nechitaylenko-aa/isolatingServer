//
// Created by artem on 28.05.26.
//

#include "../include/CFilterIonExchange.h"
#include "CContainer.h"
#include "IGeneralTor.h"
#include "../../calc/IShadowManager.h"
#include "../../../include/Logger.h"

namespace NCore
{

    CFilterIonExchange::CFilterIonExchange(IGeneralTor * tor, CCell *owner)
        : NComponent(tor, owner, EWB_WATER_FILTER_ION_EXCHANGE)
    {
        m_descript = "Filter ion ex.";
        m_schName = "ФИ" + std::to_string(m_id);
        /// для эмблемы компонента
        m_imgSource = ":/palette/images/palette/19.png";

        /// Обязательно создаём входные и выходные пины. Они и будут соединяться в сеть.
        COperatingBody ob(BT_WATER);
        auto [in, out,body] = add_ob(&ob);
        m_inputs.push_back(in);
        m_outputs.push_back(out);
        m_body = body;

        auto [in1, out1,body1] = add_ob(&ob, CT_ADDITIONAL);
        m_inputs.push_back(in1);
        m_outputs.push_back(out1);

        /// Параметры устройства по умолчанию. В дальнейшем они должны браться из оборудования.
        VSubtypes sType;
        m_throughput_capacity = new CParameter(E_MEASURE_UNITS::EMU_VOLUME, EMUVOL::emv_liter,
                                               20, EStandardPrefix::ESP_NONE, Tstring ("загруз."));
        m_throughput_capacity->set_value(20);

        m_average_filtration = new CParameter(E_MEASURE_UNITS::EMU_VOLUME, EMUVOL::emv_liter,
                                              150, EStandardPrefix::ESP_MILLI, Tstring ("фильтрация"));
        m_average_filtration->set_value(150);

        /// Наполнить параметрами для отображения в свойствах. Эти параметры передаст в GUI визуальный враппер
        m_parameters.push_back(*m_throughput_capacity);
        m_parameters.push_back(*m_average_filtration);
    }

    CFilterIonExchange::~CFilterIonExchange()
    {
        delete m_throughput_capacity;
        delete m_average_filtration;
    }

    bool CFilterIonExchange::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);
        m_inputs.at(1)->set_id(id_in1);
        m_outputs.at(1)->set_id(id_out1);
        return true;
    }

    void CFilterIonExchange::get_parameters(CContainer &container)
    {
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
        container.add_member(id_in1, "id_input_add");
        container.add_member(id_out1, "id_out_add");
    }

    Tstring CFilterIonExchange::get_description() const
    {
        return m_descript;
    }
    //------------------------------------------------------------------------------------------------------------------
    // main component mathematics --------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------------------------
    COperatingBody *CFilterIonExchange::put_ob(COperatingBody *body, CCap *sender)
    {
        // if outer injection
        if (!sender)
        {
            return m_outputs.front()->put_ob(body, nullptr);
        }
        // if input is source
        if (sender == m_inputs.front())
        {
            *m_body = *body;
            calculate_body();
            return m_outputs.front()->put_ob(m_body, nullptr);
        }
        if (sender == m_outputs.front())
        {
            return m_inputs.front()->put_ob(body, nullptr);
        }
        // if output, echo
        return CCell::put_ob(body, sender);
    }

    void CFilterIonExchange::calculate_body()
    {
        Logger &logger = Logger::instance();
        CParameter* hardness_param = nullptr;

        for (uint32_t i = 0; i < m_body->parameters_count(); ++i)
        {
            auto param = m_body->get_parameter(i);
            if (param->measure_unit()->measure_unit() == E_MEASURE_UNITS::EMU_HARDNESS)
            {
                hardness_param = param;
                break;
            }
        }

        if (!hardness_param)
        {
            logger.error("CFilterIonExchange: no hardness parameter found in operating body");
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
                logger.error("CFilterIonExchange: no parameters to request equipment");
            }
            return;
        }

        auto values = IShadowManager::getBodyParams(this, m_equipProxy, m_generalTor, m_body);

        if (!values.empty())
        {
            hardness_param->set_si_value(values.front());
        }
        else
        {
            logger.error("CFilterIonExchange: no calculations result with equipment");
        }
    }

    void CFilterIonExchange::set_equipment(equip::CEquipment *equip)
    {
        /*assert(equip != nullptr);

        if (!m_equipment)
        {
            m_equipment = equip;
        }
        else
        {
            m_equipmentChoice.push_back(equip);
        }*/
    }

    void CFilterIonExchange::set_equipmentProxy(std::vector<SEquipLight> &&items)
    {
        if (items.empty())
            return;

        m_equipmentChoice = std::move(items);
        m_equipProxy = m_equipmentChoice.at(0);
    }

    std::vector<SSignalRole> CFilterIonExchange::required_signals() const
    {
        return {
                { SR_DP_FILTER, 0, E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale, EStandardPrefix::ESP_NONE,
                  CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SCommandRole> CFilterIonExchange::required_commands() const
    {
        return {
                { CR_FLOW_DIRECTION_VALVE, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                        CS_COMPONENT_SCOPED, false, 0 },
                { CR_FLOW_DIRECTION_VALVE, 1, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                        CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SAutomationSpec> CFilterIonExchange::automation() const
    {
        auto commands = required_commands();
        return {
                // Для сорбции реальный цикл может определяться не временем, а фактом
                // насыщения загрузки (см. открытый вопрос №2) — оставлено как есть до
                // отдельного обсуждения.
                { TT_SCHEDULE, EA_NONE, ESignalRole{}, 0, /*period_seconds*/ 3u * 24u * 3600u, 0, SN_PROCEDURE,
                  commands, ESS_FIXED, 0.0f }
        };
    }

    std::vector<SDesignConstraintSpec> CFilterIonExchange::design_constraints() const
    {
        return {
                { DC_FILTRATION_VELOCITY_VS_PRESSURE,
                  "скорость фильтрации по факт. давлению не должна превышать паспортную v_max "
                  "(сорбционная загрузка)" }
        };
    }
}
