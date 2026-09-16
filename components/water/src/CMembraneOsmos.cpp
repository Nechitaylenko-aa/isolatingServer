//
// Created by artem on 28.05.26.
// Реализация к уже существующему CMembraneOsmos.h — по образцу CLightFilter.cpp.
//
// Упрощение (явно, не скрыто): реальная мембрана — 3-портовое устройство (фид ->
// пермеат + концентрат), но NComponent сейчас даёт только 1 вход/1 выход, как и
// у фильтра. Здесь трактуется как 2-портовый элемент (фид -> пермеат), концентрат/
// реджект не выведен отдельным Cap — TODO на будущее, не блокирует слой 2/3/4,
// т.к. required_commands()/required_signals() не привязаны к числу портов жёстко.
//
// Это первый компонент, где automation() объявляет EA_PID — резолюция в
// CControlLoop (а не в SAtomSet) уже решена архитектурно (automation_layer_context.md:
// "bind() резолвит EA_PID→CControlLoop"), но сам CControlLoop не написан — здесь
// декларация корректна и достаточна для CAutomationCollector, откуда она в итоге
// попадёт (не в этой сессии).
//

#include "../include/CMembraneOsmos.h"
#include "../../../include/Logger.h"
#include <cassert>

namespace NCore
{
    // TODO: EWB_WATER_MEMBRANE_OSMOS — предположение об имени константы в E_WATER_COMPONENTS.
    CMembraneOsmos::CMembraneOsmos(IGeneralTor *tor, CCell *owner)
        : NComponent(tor, owner, E_WATER_COMPONENTS::EWB_MEMBRANE_OSMOS)
    {
        assert(owner->get_owner() == nullptr);

        m_descript = "Membrane osmosis";
        m_schName = "М" + std::to_string(m_id);

        m_imgSource = ":/palette/images/palette/membrane.png"; // TODO: сверить реальный путь эмблемы

        COperatingBody ob(BT_WATER);
        auto [in, out, body] = add_ob(&ob);
        m_body = body;
        m_inputs.push_back(in);
        m_outputs.push_back(out);

        m_volume = new CParameter(E_MEASURE_UNITS::EMU_VOLUME, EMUVOL::emv_liter,
                                   0, EStandardPrefix::ESP_NONE, "производительность");
        m_volume->set_value(500); // TODO: паспортное значение по умолчанию, взято с потолка

        m_time = new CParameter(E_MEASURE_UNITS::EMU_TIME, EMUTIM::emit_hour,
                                 0, EStandardPrefix::ESP_NONE, "время цикла");
        m_time->set_value(1);

        m_parameters.push_back(*m_volume);
        m_parameters.push_back(*m_time);
    }

    CMembraneOsmos::~CMembraneOsmos()
    {
        delete m_volume;
        delete m_time;
    }

    bool CMembraneOsmos::set_parameters(CContainer &container)
    {
        auto p_type = (E_PROJECT_TYPE)NComponent::m_ser_project_type;
        m_successor_component_type = ComponentConverter::get_TComponentType_from_int(p_type, m_ser_comp_type);
        m_inputs.at(0)->set_id(id_in);
        m_outputs.at(0)->set_id(id_out);

        return true;
    }

    void CMembraneOsmos::get_parameters(CContainer &container)
    {
        container.add_member(*m_volume, "Производительность");
        container.add_member(*m_time, "Время цикла");
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

    Tstring CMembraneOsmos::get_description() const
    {
        return m_descript;
    }

    COperatingBody *CMembraneOsmos::put_ob(COperatingBody *body, CCap *sender)
    {
        // Заглушка расчёта пермеата (не цель этой сессии) — тождественное протекание,
        // как у фильтра до появления Shadow-расчёта. Реальный расчёт (recovery ratio,
        // отбраковка солей и т.п.) — предмет будущего CShadowMembraneOsmos по аналогии
        // с CShadowLightFilter, не входит в объём сегодняшней задачи (компоненты нужны
        // только для проверки кандидатов слоя 3/4, не для содержательной гидравлики).
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

    std::vector<SSignalRole> CMembraneOsmos::required_signals() const
    {
        // Давление фида — смысл сигнала специфичен именно входной стороне мембраны
        // (в отличие от DP_FILTER у фильтра, тут не разница, а абсолютное давление
        // на конкретной стороне) — поэтому CS_FLOW_SPECIFIC, is_input=true, cap_index=0.
        return {
                { SR_PRESSURE_PV, 0, E_MEASURE_UNITS::EMU_PRESSURE, EMUPRE::emp_pascale, EStandardPrefix::ESP_NONE,
                  CS_FLOW_SPECIFIC, true, 0 }
        };
    }

    std::vector<SCommandRole> CMembraneOsmos::required_commands() const
    {
        // Дроссель на линии концентрата/реджекта — регулирует давление фида и recovery
        // ratio. Cap-якорь физически на output(0) до появления отдельного Cap концентрата
        // (см. TODO вверху файла), поэтому CS_COMPONENT_SCOPED, не CS_FLOW_SPECIFIC —
        // клапан не привязан к смысловому "выходу пермеата".
        return {
                { CR_VALVE_OPEN_CLOSE, 0, E_MEASURE_UNITS::EMU_AMOUNT, EMUAMO::EA_ENUM,
                  CS_COMPONENT_SCOPED, false, 0 }
        };
    }

    std::vector<SAutomationSpec> CMembraneOsmos::automation() const
    {
        // EA_PID: bind() резолвит в CControlLoop, не в SAtomSet (см. заголовок файла).
        // setpoint — давление фида по ТЗ; ESS_TOR_OUT_PRESSURE переиспользуется здесь
        // условно (мембрана регулирует СВОЁ входное давление, не итоговое установки) —
        // TODO: возможно нужен отдельный ESetpointSource (ESS_TOR_MEMBRANE_FEED_PRESSURE
        // или подобный), не заводил новый enum-член ради одного компонента без второго
        // прецедента.
        //auto commands = required_commands();

        SAutomationSpec spec{ETriggerType::TT_CONTINUOUS_PV, EA_PID, ESignalRole::SR_PRESSURE_PV, 0, 0, 0,
                             EScheduleNature::SN_DUTY_CYCLE,{}};

        return {spec};
    }

    std::vector<SDesignConstraintSpec> CMembraneOsmos::design_constraints() const
    {
        // Аналог DC_FILTRATION_VELOCITY_VS_PRESSURE для мембраны — та же идея ("хитрован
        // поставил мощный насос"), другая физика. Переиспользую существующий тип вместо
        // заведения DC_MEMBRANE_PRESSURE_VS_RECOVERY без второго прецедента — если появится
        // третий кандидат на "давление от насоса выше паспортного", стоит завести отдельный
        // тип, сейчас это было бы преждевременной абстракцией ради одного компонента.
        return {
                { DC_FILTRATION_VELOCITY_VS_PRESSURE,
                  "давление фида по факт. напору насоса не должно превышать паспортное для мембраны" }
        };
    }
}
