//
// Created by artem on 25.05.24.
//

#include "../include/CDrinkWaterTor.h"
#include "floats.h"
#include "CMultiLanguage.h"
#include "COperatingBody.h"
//#include "CContainer.h"
//#include "CLimits.h"

extern E_NYM_LANG  current_language;

CDrinkWaterTor::CDrinkWaterTor(CSubProject *parent)
{
    m_station_params = new s_drink_water();
    *m_incomeHourMax = m_station_params->consumption_in_max_hour;
    m_parent = parent;

    m_working_bodies->emplace_back(new NCore::COperatingBody(NCore::E_BODY_TYPE::BT_WATER));
    // m_reference_bodies->emplace_back(new NCore::COperatingBody(NCore::E_BODY_TYPE::BT_WATER));

    m_limits = new std::vector<CLimits>();

    fill_working_body();
}

CDrinkWaterTor::~CDrinkWaterTor()
{
    delete m_station_params;
    delete m_limits;
    // delete m_working_bodies;       -- in the IGeneralTor
    // delete m_reference_bodies;     -- in the IGeneralTor
}

void CDrinkWaterTor::Delete()
{
    delete this;
}

NCore::E_PROJECT_TYPE CDrinkWaterTor::type() const
{
    return NCore::pt_water_drink;
}

bool CDrinkWaterTor::is_ready() const
{
    bool is_ready = !floats::is_floats_equal(m_station_params->out_press.value(), 0) &&
                    !floats::is_floats_equal(m_station_params->consumption_out_max_hour.value(), 0) &&
                    !floats::is_floats_equal(m_station_params->consumption_in_max_hour.value(), 0) &&
                    !floats::is_floats_equal(m_station_params->in_temper.value(), 0) &&
                    !floats::is_floats_equal(m_station_params->in_pressure.value(), 0) &&
                    !floats::is_floats_equal(m_station_params->consumption_in_day.value(), 0) &&
                    !(*m_reference_bodies == *m_working_bodies);
    return is_ready;
}

std::vector<NCore::COperatingBody *> * CDrinkWaterTor::operating_bodies()
{
    return m_working_bodies;
}

NCore::COperatingBody *CDrinkWaterTor::reference_body()
{
    if (!m_reference_bodies->empty())
        return m_reference_bodies->at(0);
    else
        return nullptr;
}

DRINK_WATER_SOURCE CDrinkWaterTor::water_source() const
{
    return static_cast<DRINK_WATER_SOURCE>(static_cast<int>(m_station_params->drink_water_source.value()));
}

void CDrinkWaterTor::set_water_source(const DRINK_WATER_SOURCE &water_source)
{
    m_station_params->drink_water_source.set_value(water_source);
}

float CDrinkWaterTor::clean_water_reserve_hour() const
{
    return static_cast<float>(m_station_params->time_clean_water_reserve_hour.value());
}

void CDrinkWaterTor::set_clean_water_reserve(const float &hours)
{
    m_station_params->time_clean_water_reserve_hour.set_value(hours);
}

float CDrinkWaterTor::chemical_warehouse() const
{
    return m_station_params->time_chemical_warehouse_mon.value();
}

void CDrinkWaterTor::set_chemical_warehouse(const float &month)
{
    m_station_params->time_chemical_warehouse_mon.set_value(month);
}

bool CDrinkWaterTor::is_waste_chanel() const
{
    return static_cast<bool>(static_cast<int>(m_station_params->is_waste_chanel.value()));
}

void CDrinkWaterTor::set_waste_chanel(const bool &yes_no)
{
    m_station_params->is_waste_chanel.set_value(yes_no);
}

float CDrinkWaterTor::water_in_temper() const
{
    return m_station_params->in_temper.value();
}

void CDrinkWaterTor::set_water_in_temper(const float &celsius)
{
    m_station_params->in_temper.set_value(celsius);
}

float CDrinkWaterTor::water_in_press() const
{
    return m_station_params->in_pressure.value();
}

void CDrinkWaterTor::set_water_in_press(const float &press_meter)
{
    m_station_params->in_pressure.set_value(press_meter);
}

float CDrinkWaterTor::consumption_in_hour_max() const
{
    return m_station_params->consumption_in_max_hour.value();
}

void CDrinkWaterTor::set_consumption_in_hour_max(const float &cubic_m_hour)
{
    m_station_params->consumption_in_max_hour.set_value(cubic_m_hour);
}

float CDrinkWaterTor::consumption_in_day() const
{
    return m_station_params->consumption_in_day.value();
}

void CDrinkWaterTor::set_consumption_in_day(const float &cubic_m_day)
{
    m_station_params->consumption_in_day.set_value(cubic_m_day);
}

float CDrinkWaterTor::consumption_out_hour_max() const
{
    return m_station_params->consumption_out_max_hour.value();
}

void CDrinkWaterTor::set_consumption_out_hour_max(const float &cubic_m_hour)
{
    m_station_params->consumption_out_max_hour.set_value(cubic_m_hour);
}

float CDrinkWaterTor::water_out_press() const
{
    return m_station_params->out_press.value();
}

void CDrinkWaterTor::set_water_out_press(const float &press_meter)
{
    m_station_params->out_press.set_value(press_meter);
}

NCore::COperatingBody *CDrinkWaterTor::general_working_body()
{
    return m_working_bodies->empty() ? nullptr : m_working_bodies->at(0);
}

s_drink_water *CDrinkWaterTor::parameters()
{
    return m_station_params;
}

void CDrinkWaterTor::fill_working_body()
{
    E_MEASURE_UNITS measure_unit;
    VSubtypes subtype;
    float   value,
            left_val,
            right_val;
    EStandardPrefix pref;
    Tstring str;
    E_ConditionOpers left_cond,
                    right_cond;
    int counter = 0;


    auto *w_body = general_working_body();

    if (!w_body)
    {
        throw std::runtime_error("there is no water in ToR!!!");
    }

    CMultiLanguage lang(current_language);
    auto adds = lang.items_by_path(ERootArea::ERA_MEASURES, "root/water_content");

    for (auto &rec : WT::water_drink_requirements)
    {
        std::tie(measure_unit, subtype, value, pref, str) = rec;
        str = adds.at(counter);
        auto *param = new CBodyParam(measure_unit, subtype, value, 0, 0,
                                     pref, str);
        w_body->add_parameter(param);

        counter++;
    }

    // m_working_bodies->push_back(w_body); adds the same as other

    for (auto &range : WT::wt_limits)
    {
        std::tie(left_val, left_cond, right_val, right_cond) = range;
        CLimits limit(left_cond, left_val, right_cond, right_val);
        m_limits->push_back(limit);
    }
}

std::vector<CLimits> *CDrinkWaterTor::required_limits()
{
    return m_limits;
}

CSubProject *CDrinkWaterTor::parent()
{
    return m_parent;
}

Tstring CDrinkWaterTor::name() const
{
    return m_name;
}

void CDrinkWaterTor::set_name(const Tstring &name)
{
    m_name = name;
}
/// serialization methods

bool CDrinkWaterTor::set_parameters(CContainer &container)
{
    /*container.add_member(m_station_params->consumption_in_day ,"consumption_in_day");
    container.add_member(m_station_params->consumption_in_max_hour ,"consumption_in_max_hour");
    container.add_member(m_station_params->consumption_out_max_hour ,"consumption_out_max_hour");
    container.add_member(m_station_params->in_pressure ,"in_pressure");
    container.add_member(m_station_params->in_temper ,"in_temper");
    container.add_member(m_station_params->out_press ,"out_press");
    container.add_member(m_station_params->time_chemical_warehouse_mon ,"time_chemical_warehouse_mon");
    container.add_member(m_station_params->time_clean_water_reserve_hour ,"time_clean_water_reserve_hour");
    container.add_member(m_station_params->drink_water_source, "water source");
    container.add_member(m_station_params->is_waste_chanel, "is waste channel");
    container.add_member(m_name, "Some name");
    container.add_member(limit_am, "Limit amount");


    for (auto &limit : *m_limits)
        container.add_member(limit, "Limit");

    body_am = m_working_bodies->size();
    container.add_member(body_am, "bodies amount");

    for (auto &body : *m_working_bodies)
    {
        param_count = body->parameters_count();
        container.add_member(param_count, "Body param count");

        for (int x = 0; x < body->parameters_count(); x++)
        {
            auto param = body->get_parameter(x);
            container.add_member(*param, "Parameter");
        }
    }*/

    *m_incomeHourMax = m_station_params->consumption_in_max_hour;
    if (!m_working_bodies->empty())
    {
        auto &body = m_working_bodies->at(0);
        body->set_si_pressure(m_station_params->in_pressure.si_value());
        body->get_pressure()->set_value(m_station_params->in_pressure.value());
        body->set_si_temperature(m_station_params->in_temper.si_value());
        body->set_si_volume(m_station_params->consumption_in_max_hour.si_value());
    }

    return true;
}

void CDrinkWaterTor::get_parameters(CContainer &container)
{
    container.add_member(m_station_params->consumption_in_day ,"consumption_in_day");
    container.add_member(m_station_params->consumption_in_max_hour ,"consumption_in_max_hour");
    container.add_member(m_station_params->consumption_out_max_hour ,"consumption_out_max_hour");
    container.add_member(m_station_params->in_pressure ,"in_pressure");
    container.add_member(m_station_params->in_temper ,"in_temper");
    container.add_member(m_station_params->out_press ,"out_press");
    container.add_member(m_station_params->time_chemical_warehouse_mon ,"time_chemical_warehouse_mon");
    container.add_member(m_station_params->time_clean_water_reserve_hour ,"time_clean_water_reserve_hour");
    container.add_member(m_station_params->drink_water_source, "water source");
    container.add_member(m_station_params->is_waste_chanel, "is waste channel");
    container.add_member(m_name, "Some name");
    container.add_member(*m_limits, "limits");
    container.add_member(limit_am, "Limits");
    container.add_member(m_station_params->amount_inputs_, "inputs amount");
    container.add_member(m_station_params->amount_outputs, "outputs amount");

    if (!container.is_deserialize())
    {
        serialize_bodies(container);
    }
    else
    {
        deserialize_bodies(container);
    }

}

CLimits *CDrinkWaterTor::limit_at(const uint8_t &index)
{
    if (index >= m_limits->size())
        return nullptr;
    return &m_limits->at(index);
}

uint8_t CDrinkWaterTor::limits_count() const
{
    return m_limits->size();
}

uint8_t CDrinkWaterTor::base_inputs() const
{
    return static_cast<uint8_t>(m_station_params->amount_inputs_.value());
}

void CDrinkWaterTor::set_base_inputs(uint8_t inputs)
{
    m_station_params->amount_inputs_.set_value(static_cast<float>(inputs));
}

uint8_t CDrinkWaterTor::base_outputs() const
{
    return static_cast<uint8_t>(m_station_params->amount_outputs.value());
}

void CDrinkWaterTor::set_base_outputs(uint8_t outputs)
{
    m_station_params->amount_outputs.set_value(static_cast<float>(outputs));
}

void CDrinkWaterTor::serialize_bodies(CContainer &container)
{
    m_incomeHourMax->set_si_value(m_station_params->consumption_in_max_hour.si_value());
    /// 1. save bodies amount
    w_body_am = m_working_bodies->size();
    r_body_am = m_reference_bodies->size();
    limit_am  = m_limits->size();
    container.add_member(w_body_am, "working bodies amount");
    container.add_member(r_body_am, "reference bodies amount");
    container.add_member(limit_am);

    w_body_params.clear();
    w_body_types.clear();

    /// 2. save working bodies param count and types
    for (auto &body : *m_working_bodies)
    {
        w_body_params.push_back(body->parameters_count());
        container.add_member(w_body_params.back());
        w_body_types.push_back(body->body_type());
        container.add_member(w_body_types.back());
    }
    /// 3. save reference bodies param count and types
    r_body_params.clear();
    r_body_types.clear();
    for (auto &body : *m_reference_bodies)
    {
        r_body_params.push_back(body->parameters_count());
        container.add_member(r_body_params.back());
        r_body_types.push_back(body->body_type());
        container.add_member(r_body_types.back());
    }

    /// 4. save working bodies itself
    uint32_t index = 0;
    for (auto &body : *m_working_bodies)
    {
        for (uint32_t i = 0; i < w_body_params.at(index); i++)
        {
            auto param = body->get_parameter(i);
            container.add_member(*param);
        }
        index++;
    }
    /// 5. save reference bodies itself
    for (auto &body : *m_reference_bodies)
    {
        for(uint32_t i = 0; i < body->parameters_count(); i++)
        {
            auto param = body->get_parameter(i);
            container.add_member(*param);
        }
        index++;
    }
    /// 6 save limits

    for (uint32_t i = 0; i < limit_am; i++)
    {
        //auto & limit = m_limits->at(i);
        container.add_member(m_limits->at(i));
    }
}

void CDrinkWaterTor::deserialize_bodies(CContainer &container)
{
    /// 1. bodies amount
    container.add_member(w_body_am, "working bodies amount");
    container.add_member(r_body_am, "reference bodies amount");
    container.add_member(limit_am, "Limits amount");
    container.set_total_times(2);
    if (!container.is_contains_variadik())
    {
        container.set_variadik(true);
        return;
    }

    /// remove old bodies =========================================
    {
        for (auto &body: *m_working_bodies)
        {
            delete body;
        }
        m_working_bodies->clear();
        for (auto &body: *m_reference_bodies)
        {
            delete body;
        }
        m_reference_bodies->clear();
    }
    /// fill body param amount and types ==============================================================
    if (container.current_time() == 0)
    {
        w_body_params.resize(w_body_am);
        w_body_types.resize(w_body_am);
        r_body_params.resize(r_body_am);
        r_body_types.resize(r_body_am);
    }
    /// 2. load working bodies param count and types
    for (uint32_t idx = 0; idx < w_body_am; idx++)
    {
        container.add_member(w_body_params.at(idx));
        container.add_member(w_body_types.at(idx));
    }
    /// 3. load reference bodies param count and types
    for (uint32_t idx = 0; idx < r_body_am; idx++)
    {
        container.add_member(r_body_params.at(idx));
        container.add_member(r_body_types.at(idx));
    }

    if (container.current_time() == 0)
    {
        container.set_current_time(1);
        return;
    }

    /// 4. load working bodies itself
    for (uint32_t i = 0; i < w_body_am; i++)
    {
        auto operBody = new NCore::COperatingBody((NCore::E_BODY_TYPE) w_body_types.at(i));
        m_working_bodies->push_back(operBody);

        for (uint32_t idx = 0; idx < w_body_params.at(i); idx++)
        {
            auto param = new CBodyParam();
            operBody->add_parameter(param);
            container.add_member(*operBody->get_parameter(idx));
        }
    }
    /// 5. load working bodies itself
    for (uint32_t i = 0; i < r_body_am; i++)
    {
        auto refBody = new NCore::COperatingBody((NCore::E_BODY_TYPE) r_body_types.at(i));
        m_reference_bodies->push_back(refBody);

        for (uint32_t idx = 0; idx < r_body_params.at(i); idx++)
        {
            auto param = new CBodyParam();
            refBody->add_parameter(param);
            container.add_member(*refBody->get_parameter(idx));
        }
    }
    /// 6. load limits
    m_limits->clear();
    m_limits->resize(limit_am);
    for (uint32_t i = 0; i < limit_am; i++)
    {
        container.add_member(m_limits->at(i));
    }

    container.set_current_time(2);
    m_incomeHourMax->set_si_value(m_station_params->consumption_in_max_hour.si_value());
}
