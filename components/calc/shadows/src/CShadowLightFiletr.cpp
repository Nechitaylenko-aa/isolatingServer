//
// Created by artem on 16.06.26.
//

#include "../include/CShadowLightFiletr.h"
#include "CParameter.h"
#include "CLimits.h"
#include "Logger.h"
#include "IGeneralTor.h"
#include "COperatingBody.h"
#include "NComponent.h"
#include "../../../water/include/CLightFilter.h"   // EFilterMediaType
//#include "CEquipmentLightFilterProxy.h"
//#include "CEquipmentLightFilter.h"


CShadowLightFilter::CShadowLightFilter(NCore::NComponent *component) : IShadow(component) {}

namespace
{
    // Па/(м/с) — гидравлическое сопротивление фильтра, см. calculateVelocityFromPressure ниже.
    // Вынесено сюда, а не оставлено локальной переменной внутри неё, потому что max_allowed_pressure_pa()
    // считает ОБРАТНУЮ величину той же зависимости и не должен держать вторую копию константы.
    constexpr float HYDRAULIC_RESISTANCE_PA_PER_MS = 10000.0f;
}

static float calculateVelocityFromPressure(float pressure, float flow_rate)
{
    // Оцениваем скорость, которую создает насос при данном давлении
    // Это упрощенная модель - если давление превышает расчетное,
    // значит насос "продавливает" фильтр

    // Расчетная скорость от давления
    float v_pressure = pressure / HYDRAULIC_RESISTANCE_PA_PER_MS;  // м/с

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

std::map<E_MEASURE_UNITS, CShadowLightFilter::SParamWithLimit>
CShadowLightFilter::collect_tracked_params(NCore::COperatingBody *body, IGeneralTor *tor) const
{
    // ВНИМАНИЕ: только мутность и цветность — смысл/вкус вынесены в сорбционный фильтр (см. чат),
    // осветлительный фильтр их не рассматривает вовсе.
    std::map<E_MEASURE_UNITS, SParamWithLimit> tracked {
            {EMU_TURBIDITY, {}},
            {EMU_CHROMATICITY, {}},
    };

    for (uint32_t i = 0; i < body->parameters_count(); ++i)
    {
        auto param = body->get_parameter(i);
        E_MEASURE_UNITS measure_unit = param->measure_unit()->measure_unit();

        auto it = tracked.find(measure_unit);
        if (it == tracked.end())
        {
            continue;
        }

        it->second.param = param;
        auto range = tor->limit_at(i);
        if (range && range->limits() && !range->limits()->empty())
        {
            it->second.limits = range;
        }
    }

    return tracked;
}

NCore::SEquipmentRequest
CShadowLightFilter::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    // ============================================================
    // 1. Получение входных данных
    // ============================================================
    auto tracked = collect_tracked_params(body, tor);

    float m_Q = tor->hourInputMax()->si_value();  // м³/ч
    float m_pressure_pa = body->get_si_pressure();
    float temp = body->get_si_temperature();        // °C

    bool is_work = false;
    for (const auto &kv : tracked)
    {
        if (kv.second.limits)
        {
            is_work = true;
            break;
        }
    }

    if (!is_work) {
        return {};
    }

    Logger &logger = Logger::instance();
    logger.init("log.txt", ELogLevel::LOG_ALL, true, false);

    // ============================================================
    // 2. Требования на реагентную подготовку — по каждому превышенному параметру отдельно.
    //    Доза (required_dose) пока не считается — нужна таблица дозирования (СНиП 2.04.02-84/СП 45.13330
    //    или аналог), откладываем на потом (см. чат). Здесь же — тир превышения ("очень высокая") по
    //    практическим порогам из документа технолога: сигнализируем через штатный механизм
    //    предупреждений компонента, реагент всё равно один и тот же (коагулянт).
    // ============================================================
    struct SReagentTierInfo
    {
        NCore::EReagentType reagent_type;
        float               very_high_threshold;   // "красная зона" — см. документ технолога
        const char *        very_high_message;
    };

    static const std::map<E_MEASURE_UNITS, SReagentTierInfo> reagent_info = {
            {EMU_TURBIDITY,    {NCore::EReagentType::Coagulant, 25.0f,
                    "Very high turbidity: risk of filter clogging, increase coagulant dose or add pre-settling."}},
            {EMU_CHROMATICITY, {NCore::EReagentType::Coagulant, 60.0f,
                    "Very high chromaticity: increased coagulant dose likely needed, consider oxidation (ozone/KMnO4) before coagulation."}},
    };
    // альтернативные (менее консервативные) пороги из того же документа: 30 по мутности, 80 по цветности —
    // если по практике лучше ложатся, поменять здесь же, больше нигде не завязано.

    for (const auto &[measure_unit, info] : reagent_info)
    {
        const auto &entry = tracked[measure_unit];
        if (!entry.param || !entry.limits)
        {
            continue;
        }

        float value = entry.param->value();
        if (!entry.limits->is_acceptable(value))
        {
            NCore::SReagentRequirement req;
            req.source = m_component;
            req.reagent_type = info.reagent_type;
            req.trigger_parameter = measure_unit;
            req.required_dose = 0.f;  // TODO: формула дозы — нужна от технолога
            m_component->info_bus()->addRequirement(std::move(req));

            if (value > info.very_high_threshold)
            {
                m_component->set_error(EComponentError::ECE_WARNING);
                m_component->set_warnMessage(info.very_high_message);
            }
        }
    }

    // ============================================================
    // 3. Определение нормативной скорости фильтрации
    // ============================================================
    float v_filtration_norm = 7.0f;  // м/ч - базовое значение
    /* Корректировка по мутности отключена — пороги (50/20/5) в исходнике не были ничем обоснованы,
       нужны от технолога, прежде чем включать обратно:
    if (turbidity_in > 50.0f) v_filtration_norm = 5.5f;
    else if (turbidity_in > 20.0f) v_filtration_norm = 6.5f;
    else if (turbidity_in < 5.0f) v_filtration_norm = 8.0f;
    */

    // ============================================================
    // 4. Формируем запрос на подбор оборудования (само оборудование, не реагент —
    //    тип загрузки/крупность каталог решает сам и вернёт их в SEquipLight::params,
    //    см. getCalculationsWithEquip)
    // ============================================================
    NCore::SEquipmentRequest req;
    req.sender = m_component;
    req.id_equip = 0;

    req.params = {
            m_Q,                // [0]  м³/ч - расход
            m_pressure_pa,      // [1]  Па - рабочее давление
            v_filtration_norm,  // [2]  м/ч - нормативная скорость (критерий подбора)
            temp,               // [3]  °C - температура
    };

    return req;
}

std::vector<float>
CShadowLightFilter::getCalculationsWithEquip(NCore::SEquipLight & equip_proxy, NCore::COperatingBody *body,
                                             IGeneralTor *tor)
{
    std::vector<float> res;

    auto tracked = collect_tracked_params(body, tor);
    const auto &turbidity_entry = tracked[EMU_TURBIDITY];
    const auto &chromaticity_entry = tracked[EMU_CHROMATICITY];

    if (!turbidity_entry.param && !chromaticity_entry.param)
    {
        return res;  // фильтру нечего обрабатывать в этом теле
    }

    float m_Q = tor->hourInputMax()->si_value();  // м³/ч
    float m_pressure = body->get_si_pressure();   // Па
    float temp = body->get_si_temperature();      // °C

    EComponentState compState = m_component->component_state(), c_state{EComponentState::ECS_DEFAULT};
    EComponentError compError = m_component->component_error(), c_error{EComponentError::ECE_NORM};

    // 3.1 Получаем параметры установленного фильтра.
    if (equip_proxy.params.size() < 4)
    {
        fprintf(stderr, "Received less then 4 fields: %zu\n", equip_proxy.params.size());
        return res;
    }
    float diameter = equip_proxy.params.at(0);           // м
    float H_layer = equip_proxy.params.at(1);            // м
    float v_max = equip_proxy.params.at(2);
    float v_min = v_max - 0.5f;
    EFilterMediaType media_type = (EFilterMediaType)equip_proxy.params.at(3);

    // 3.2 Расчет фактической площади и скорости
    float F_filter = 3.14159f * diameter * diameter / 4.0f;  // м²
    float v_filtration_actual = m_Q / F_filter;  // м/ч

    // 3.3 КРИТИЧЕСКАЯ ПРОВЕРКА: скорость от давления
    float v_from_pressure = calculateVelocityFromPressure(m_pressure, m_Q);

    Logger &logger = Logger::instance();
    Tstring warning_message;

    if (v_from_pressure > v_max * 1.1f) {  // 10% запас
        warning_message = "Filtration speed exceeds physical limit. Install flow restrictor or reduce pump power.";
        c_error = EComponentError::ECE_CRITICAL;
        logger.critical(warning_message);
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

    // 3.5 Расчет эффективности с учетом ограничений
    float efficiency = 0.75f + (7.0f - v_filtration_actual) * 0.03f;

    float speed_penalty = 1.0f;
    if (v_filtration_actual > 7.0f)
    {
        speed_penalty = 1.0f - (v_filtration_actual - 7.0f) * 0.02f;
        speed_penalty = std::max(speed_penalty, 0.85f);
        efficiency *= speed_penalty;
    }

    // Корректировка по мутности (если параметр вообще есть в этом теле)
    if (turbidity_entry.param)
    {
        float turbidity_in = turbidity_entry.param->si_value();
        if (turbidity_in > 50.0f) {
            efficiency *= 0.9f;
        } else if (turbidity_in < 5.0f) {
            efficiency *= 1.05f;
        }
    }

    // Корректировка по температуре
    if (temp < 10.0f) {
        efficiency *= 0.95f;
    } else if (temp > 25.0f) {
        efficiency *= 1.02f;
    }

    if (media_type == EMF_ANTHRACITE) {
        efficiency *= 1.05f;
    }
    else if (media_type == EMF_EXPANDED_CLAY) {
        efficiency *= 1.08f;
    }
    else if (media_type == EMF_ZEOLITE) {
        efficiency *= 1.10f;
    }

    // Поправка на высоту слоя
    if (H_layer > 0.0f) {
        float height_factor = 1.0f + (H_layer - 1.0f) * 0.05f;
        efficiency *= std::clamp(height_factor, 0.9f, 1.15f);
    }

    efficiency = std::clamp(efficiency, 0.4f, 0.95f);

    // 3.6 Выходные значения — мутность и цветность.
    // ВНИМАНИЕ: цветность считается той же efficiency, что и мутность — это допущение,
    // не проверенное технологом (см. чат). Возможно, у цветности своя, отдельная эффективность
    // снятия через одну и ту же загрузку.
    float turbidity_out = turbidity_entry.param
            ? turbidity_entry.param->si_value() * (1.0f - efficiency) : 0.f;
    float chromaticity_out = chromaticity_entry.param
            ? chromaticity_entry.param->si_value() * (1.0f - efficiency) : 0.f;

    res.push_back(turbidity_out);
    res.push_back(chromaticity_out);

    m_last_calculation.has_data = true;
    m_last_calculation.has_turbidity = (turbidity_entry.param != nullptr);
    m_last_calculation.has_chromaticity = (chromaticity_entry.param != nullptr);
    m_last_calculation.turbidity_in = turbidity_entry.param ? turbidity_entry.param->si_value() : 0.f;
    m_last_calculation.turbidity_out = turbidity_out;
    m_last_calculation.chromaticity_in = chromaticity_entry.param ? chromaticity_entry.param->si_value() : 0.f;
    m_last_calculation.chromaticity_out = chromaticity_out;
    m_last_calculation.efficiency = efficiency;
    m_last_calculation.turbidity_unit = turbidity_entry.param ? turbidity_entry.param->dimension_si_name() : "";
    m_last_calculation.chromaticity_unit = chromaticity_entry.param ? chromaticity_entry.param->dimension_si_name() : "";
    // ВНИМАНИЕ: единицы — SI (как и все числа здесь), не обязательно то же, что привычно видеть
    // в бумажном отчёте (например, мутность обычно в ЕМФ). Если си-единица не совпадает с
    // "человеческой" — понадобится величина в value(), а не si_value(), здесь и в формулах ниже.

    m_last_calculation.filter_area = F_filter;
    m_last_calculation.v_max = v_max;

    // Интенсивность промывки (i) — от технолога есть только для антрацита (10-12 л/(с·м²), беру
    // середину). Для керамзита/цеолита данных не было; для кварцевого песка (15-18 л/(с·м²), тоже
    // есть от технолога) в EFilterMediaType вообще нет соответствующего значения загрузки — enum
    // ограничен тремя типами. Не выдумываю — считаем объём промывки только там, где известна i.
    switch (media_type)
    {
        case EMF_ANTHRACITE:
            m_last_calculation.has_backwash_intensity = true;
            m_last_calculation.backwash_intensity = 11.0f;  // л/(с·м²), середина 10-12
            break;
        default:
            m_last_calculation.has_backwash_intensity = false;
            break;
    }

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

std::optional<float> CShadowLightFilter::backwash_volume_required() const
{
    if (!m_last_calculation.has_data || !m_last_calculation.has_backwash_intensity)
    {
        return std::nullopt;
    }

    constexpr float t_minutes = 20.0f;  // фиксировано технологом, для всех фильтров водой
    constexpr float k = 0.06f;          // 60 сек / 1000 л, см. чат

    return k * m_last_calculation.backwash_intensity * t_minutes * m_last_calculation.filter_area;
}

std::optional<float> CShadowLightFilter::max_allowed_pressure_pa() const
{
    if (!m_last_calculation.has_data || m_last_calculation.v_max <= 0.f)
    {
        return std::nullopt;
    }

    // Тот же 10%-запас и та же гидравлика, что и в проверке "Filtration speed exceeds physical
    // limit" внутри getCalculationsWithEquip — насос не должен создавать давление, которое даёт
    // скорость выше v_max*1.1.
    float v_threshold_m_h = m_last_calculation.v_max * 1.1f;
    float v_threshold_m_s = v_threshold_m_h / 3600.0f;
    return v_threshold_m_s * HYDRAULIC_RESISTANCE_PA_PER_MS;
}

std::vector<SReportEntry> CShadowLightFilter::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action,
                                                              const Tstring &section_number, uint32_t & formula_start)
{
    // body/tor намеренно не используются: отчёт строится по снимку последнего расчёта
    // (m_last_calculation), а не пересчитывается заново — см. комментарий у SLastCalculation.
    // Следствие: если тело изменилось после последнего getCalculationsWithEquip(), отчёт
    // покажет прошлый расчёт, а не текущее состояние — пока это не проблема (отчёт создаётся
    // по факту произошедшего расчёта), но стоит держать в уме.
    std::vector<SReportEntry> report;

    if (!m_last_calculation.has_data)
    {
        return report;  // расчёта ещё не было — отчитываться не о чем
    }

    auto fmt = [](float v) -> Tstring
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", v);
        return Tstring(buf);
    };

    uint32_t formula_no = formula_start;

    SReportEntry heading;
    heading.kind = EReportEntryKind::SectionHeading;
    heading.text = section_number + " Расчет фильтра осветлительного " + m_component->schematicName();
    report.push_back(heading);

    if (m_last_calculation.has_turbidity)
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Formula;
        e.parameter_name = "Мутность на выходе";
        e.formula_symbolic = "C_out = C_in * (1 - eta)";
        e.substituted = "C_out = " + fmt(m_last_calculation.turbidity_in) + " * (1 - "
                       + fmt(m_last_calculation.efficiency) + ")";
        e.result = m_last_calculation.turbidity_out;
        e.unit = m_last_calculation.turbidity_unit;
        e.formula_number = formula_no++;
        report.push_back(e);
    }

    if (m_last_calculation.has_chromaticity)
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Formula;
        e.parameter_name = "Цветность на выходе";
        e.formula_symbolic = "C_out = C_in * (1 - eta)";
        e.substituted = "C_out = " + fmt(m_last_calculation.chromaticity_in) + " * (1 - "
                       + fmt(m_last_calculation.efficiency) + ")";
        e.result = m_last_calculation.chromaticity_out;
        e.unit = m_last_calculation.chromaticity_unit;
        e.formula_number = formula_no++;
        report.push_back(e);
    }

    if (auto backwash = backwash_volume_required())
    {
        SReportEntry e;
        e.kind = EReportEntryKind::Formula;
        e.parameter_name = "Объём воды на одну обратную промывку";
        e.formula_symbolic = "V = 0.06 * i * t * F";
        e.substituted = "V = 0.06 * " + fmt(m_last_calculation.backwash_intensity) + " * 20 * "
                       + fmt(m_last_calculation.filter_area);
        e.result = *backwash;
        e.unit = "м3";
        e.formula_number = formula_no++;
        report.push_back(e);
    }

    return report;
}
