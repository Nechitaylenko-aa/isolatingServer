//
// Created by artem on 16.06.26.
//

#include "../include/CShadowLightFiletr.h"
#include "CParameter.h"
#include "Logger.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"
#include "CEquipmentLightFilterProxy.h"
#include "CEquipmentLightFilter.h"


CShadowLightFilter::CShadowLightFilter(NCore::NComponent *component) : IShadow(component), m_component(component) {}

static float calculateVelocityFromPressure(float pressure, float flow_rate)
{
    // Оцениваем скорость, которую создает насос при данном давлении
    // Это упрощенная модель - если давление превышает расчетное,
    // значит насос "продавливает" фильтр

    // Сопротивление фильтра (гидравлическое) - упрощенно
    // При нормальных условиях: ~10 кПа на 1 м/с скорости
    float hydraulic_resistance = 10000.0f;  // Па/(м/с) - для песчаного фильтра

    // Расчетная скорость от давления
    float v_pressure = pressure / hydraulic_resistance;  // м/с

    // Переводим в м/ч
    v_pressure *= 3600.0f;  // м/ч

    // Корректировка по расходу
    // Если расход мал, а давление высокое - явно что-то не так
    float expected_pressure_for_flow = flow_rate * 500.0f;  // упрощенно

    if (pressure > expected_pressure_for_flow * 1.5f) {
        // Давление значительно выше расчетного - насос слишком мощный
        v_pressure *= 1.2f;  // увеличиваем оценку
    }

    return v_pressure;
}

NCore::SEquipmentRequest
CShadowLightFilter::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    // ============================================================
    // 1. Получение входных данных
    // ============================================================
    struct params_set {
        CParameter *     param{nullptr};
        CLimits     *    limits{nullptr};
    };

    std::map<E_MEASURE_UNITS, params_set> processing_params {
            {EMU_TURBIDITY, {}},
            {EMU_CHROMATICITY, {}},
            { EMU_SMELL, {}},
            { EMU_FLAVOR , {}}};

    float m_Q = tor->hourInputMax()->si_value();  // м³/ч
    float m_pressure_pa = body->get_si_pressure();
    float temp = body->get_si_temperature();        // °C
    float turbidity_need = -1.f;//tor->limit_at(4)->limits()->at(0).right_limit;
    bool  is_work{false};

    CParameter* turbidity_param = nullptr;
    for (uint32_t i = 0; i < body->parameters_count(); ++i)
    {
        auto param = body->get_parameter(i);
        E_MEASURE_UNITS measure_unit = param->measure_unit()->measure_unit();

        auto it = processing_params.find(measure_unit);
        if (it != processing_params.end())
        {
            it->second.param = param;
            auto range = tor->limit_at(i);
            if (range && range->limits() && !range->limits()->empty())
            {
                it->second.limits = range;
                is_work = true;
            }
        }
    }

    if (!is_work) {
        return {};
    }

    //float turbidity_in = processing_params[EMU_TURBIDITY].param->si_value();//turbidity_param->si_value();  // мг/л или NTU

    if (processing_params[EMU_TURBIDITY].param)
    {
        if (!processing_params[EMU_TURBIDITY].limits->
                is_acceptable(processing_params[EMU_TURBIDITY].param->value()))
        {
            // расчет фильтра для мутности
        }
    }
    if (processing_params[EMU_CHROMATICITY].param)
    {
        if (!processing_params[EMU_CHROMATICITY].limits->
                is_acceptable(processing_params[EMU_CHROMATICITY].param->value()))
        {
            // расчет фильтра по цветности
        }
    }
    // и так далее

    Logger &logger = Logger::instance();
    logger.init("log.txt", ELogLevel::LOG_ALL, true, false);

    // 2.1 Определение нормативной скорости фильтрации
    float v_filtration_norm = 7.0f;  // м/ч - базовое значение
    /*
    // Корректировка по мутности
    if (turbidity_in > 50.0f)
    {
        v_filtration_norm = 5.5f;
    }
    else if (turbidity_in > 20.0f)
    {
        v_filtration_norm = 6.5f;
    }
    else if (turbidity_in < 5.0f) {
        v_filtration_norm = 8.0f;
    }
    */

    // 2.5 Формируем запрос на подбор оборудования
    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;

    /*req.params = {
            m_Q,                            // [0]  м³/ч - расход
            m_pressure_pa,                     // [2]  Па - рабочее давление
            required_diameter,              // [1]  м - требуемый диаметр
            //temp,                           // [3]  °C - температура
            //static_cast<float>(media_code), // [4]  тип загрузки
            //grain_size,                     // [5]  мм - размер зерен
            layer_height,                   // [6]  м - высота слоя
            //static_cast<float>(layer_count),// [7]  количество слоев
            //v_filtration_norm,              // [8]  м/ч - нормативная скорость (для проверки)
            //v_max_physical,                 // [9]  м/ч - физический предел скорости
    };*/

    return req;
}

std::vector<float>
CShadowLightFilter::getCalculationsWithEquip(NCore::SEquipLight & equip_proxy, NCore::COperatingBody *body,
                                             IGeneralTor *tor)
{
    std::vector<float> res;

    float m_Q = tor->hourInputMax()->si_value();  // м³/ч
    float m_pressure = body->get_si_pressure();   // Па
    float temp = body->get_si_temperature();      // °C
    float turbidity_need = 1.5f;

    CParameter* turbidity_param = nullptr;
    for (uint32_t i = 0; i < body->parameters_count(); ++i)
    {
        auto param = body->get_parameter(i);
        Tstring pName = param->unit_name();
        if (pName == Mutnost)
        {
            turbidity_param = param;
            auto ranges = tor->limit_at(i);
            turbidity_need = ranges->limits()->front().right_limit;
            break;
        }
    }

    if (!turbidity_param)
    {
        return res;
    }

    float turbidity_in = turbidity_param->si_value();

    EComponentState compState = m_component->component_state(), c_state{EComponentState::ECS_DEFAULT};
    EComponentError compError = m_component->component_error(), c_error{EComponentError::ECE_NORM};

    // 3.1 Получаем параметры установленного фильтра. Компонент знает к чему кастовать оборудование
    if (equip_proxy.params.size() < 3)
    {
        fprintf(stderr, "Received less then 3 fields: %zu\n", equip_proxy.params.size());
    }
    assert(equip_proxy.params.size() >=3);
    float diameter = equip_proxy.params.at(0);//equipt->diameter()->si_value();     // м
    float H_layer = equip_proxy.params.at(1);//equipt->bedDepth()->si_value();   // м
    float v_max = equip_proxy.params.at(2);//equipt->maxFlowRate()->value();
    float v_min = v_max - 0.5f;
    EFilterMediaType media_type = (EFilterMediaType)equip_proxy.params.at(3);//equipt->chosenMedia();
    //float d_grain = equipment->grainSize();     // not using

    // 3.2 Расчет фактической площади и скорости
    float F_filter = 3.14159f * diameter * diameter / 4.0f;  // м²
    float v_filtration_actual = m_Q / F_filter;  // м/ч

    // 3.3 КРИТИЧЕСКАЯ ПРОВЕРКА: скорость от давления
    //     Если скорость превышает паспортную - значит насос слишком мощный
    float v_from_pressure = calculateVelocityFromPressure(m_pressure, m_Q);

    Logger &logger = Logger::instance();
    Tstring warning_message;



    if (v_from_pressure > v_max * 1.1f) {  // 10% запас
        // Хитрован поставил мощный насос!
        warning_message = "Filtration speed exceeds physical limit. Install flow restrictor or reduce pump power.";
        c_error = EComponentError::ECE_CRITICAL;
        logger.critical(warning_message);
        //return res;
    }

    // 3.4 Проверка скорости в допустимых пределах
    if (v_filtration_actual > v_max)
    {
        warning_message = "WARNING: Speed exceeds maximum!";
        c_error = EComponentError::ECE_WARNING;
        v_filtration_actual = v_max;
    } else if (v_filtration_actual < v_min)
    {
        warning_message = "INFO: Speed below optimum.";
        c_error = EComponentError::ECE_NORM;
    } else
    {
        warning_message = "OK";
        c_error = EComponentError::ECE_NORM;
    }

    // 3.5 Расчет эффективности с учетом ОГРАНИЧЕНИЙ
    float efficiency = 0.75f + (7.0f - v_filtration_actual) * 0.03f;

    // ШТРАФ за превышение скорости (даже если в допустимых пределах)
    float speed_penalty = 1.0f;
    if (v_filtration_actual > 7.0f)
    {
        speed_penalty = 1.0f - (v_filtration_actual - 7.0f) * 0.02f;
        speed_penalty = std::max(speed_penalty, 0.85f);
        efficiency *= speed_penalty;
    }

    // Корректировка по мутности
    if (turbidity_in > 50.0f)
    {
        efficiency *= 0.9f;
    }
    else if (turbidity_in < 5.0f)
    {
        efficiency *= 1.05f;
    }

    // Корректировка по температуре
    if (temp < 10.0f) {
        efficiency *= 0.95f;
    } else if (temp > 25.0f) {
        efficiency *= 1.02f;
    }

    if (media_type == EMF_ANTHRACITE) {
        efficiency *= 1.05f;   // Антрацит: выше грязеемкость, лучше задерживает взвесь
    }
    else if (media_type == EMF_EXPANDED_CLAY) {
        efficiency *= 1.08f;   // Керамзит: высокая пористость, хорошая грязеемкость
    }
    else if (media_type == EMF_ZEOLITE) {
        efficiency *= 1.10f;   // Цеолит: дополнительный сорбционный эффект
    }

    // Поправка на высоту слоя
    if (H_layer > 0.0f) {
        float height_factor = 1.0f + (H_layer - 1.0f) * 0.05f;
        efficiency *= std::clamp(height_factor, 0.9f, 1.15f);
    }

    efficiency = std::clamp(efficiency, 0.4f, 0.95f);

    // 3.6 Выходная мутность
    float turbidity_out = turbidity_in * (1.0f - efficiency);
    res.push_back(turbidity_out);

    if (c_error != compError)
    {
        m_component->set_error(c_error);
        m_component->set_warnMessage(warning_message);
    }
    if (c_state != compState)
    {
        m_component->set_state(c_state);
    }

    return res;
}

void CShadowLightFilter::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action)
{

}
