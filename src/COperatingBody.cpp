//
// Created by artem on 04.04.24.
//

#include <stdexcept>
#include <cassert>
#include "../include/COperatingBody.h"


namespace NCore
{
    COperatingBody::COperatingBody(const E_BODY_TYPE &body_type)
    {
        m_body_type = body_type;

        m_temperature = new CBodyParam(EMU_TEMPERATURE, EMUTEM::emt_celsius);
        m_pressure    = new CBodyParam(EMU_PRESSURE, EMUPRE::emp_pascale,
                                       0, 0, 0, ESP_MEGA);
        m_volume      = new CBodyParam(EMU_VOLUME, EMUVOL::emv_meter);

        m_body_parameters = new std::vector<CBodyParam*>();
    }

    COperatingBody::COperatingBody(const COperatingBody & src)
    {
        m_body_type   = src.m_body_type;
        m_is_test = src.m_is_test;

        m_temperature = new CBodyParam(*src.m_temperature);//(EMU_TEMPERATURE, EMUTEM::emt_celsius);
        m_pressure    = new CBodyParam(*src.m_pressure);//(EMU_PRESSURE, EMUPRE::emp_pascale);
        m_volume      = new CBodyParam(*src.m_volume);//(EMU_VOLUME, EMUVOL::emv_meter);

        m_body_parameters = new std::vector<CBodyParam*>();

        for (auto &alien : *src.m_body_parameters)
        {
            m_body_parameters->emplace_back(new CBodyParam(*alien));
        }
    }

    COperatingBody::COperatingBody(COperatingBody && ref) noexcept
      : m_temperature(ref.m_temperature)
      , m_volume(ref.m_volume)
      , m_pressure(ref.m_pressure)
      , m_body_type(ref.m_body_type)
      , m_body_parameters(ref.m_body_parameters)
      , m_is_test(ref.m_is_test)

    {
        ref.m_temperature = nullptr;
        ref.m_volume = nullptr;
        ref.m_pressure = nullptr;
        ref.m_body_parameters = nullptr;

    }

    COperatingBody::~COperatingBody()
    {
        delete m_temperature;
        delete m_pressure;
        delete m_volume;


        if (m_body_parameters)
        {
            for (auto &item : *m_body_parameters)
            {
                delete item;
            }
        }

        delete m_body_parameters;
    }

    COperatingBody& COperatingBody::operator=(COperatingBody && tmp) noexcept
    {
        if (this != &tmp)
        {
            delete m_temperature;
            delete m_pressure;
            delete m_volume;


            reset_body();
            delete m_body_parameters;

            m_temperature = std::exchange(tmp.m_temperature, nullptr);
            m_pressure    = std::exchange(tmp.m_pressure, nullptr);
            m_volume      = std::exchange(tmp.m_volume, nullptr);
            m_body_parameters = std::exchange(tmp.m_body_parameters, nullptr);

            m_is_test = tmp.m_is_test;
        }

        return *this;
    }

    COperatingBody & COperatingBody::operator=(const COperatingBody & rhs)
    {
        assert(this->body_type() == rhs.body_type());

        if (&rhs == this)
        {
            return *this;
        }

        reset_body();

        for (auto &alien : *rhs.m_body_parameters)
        {
            m_body_parameters->emplace_back(new CBodyParam(*alien));
        }

        m_pressure->set_value(rhs.m_pressure->value());
        m_temperature->set_value(rhs.m_temperature->value());
        m_volume->set_value(rhs.m_volume->value());
        m_is_test        = rhs.m_is_test;

        return *this;
    }

    COperatingBody COperatingBody::operator+(const COperatingBody &rhs)
    {
        //delete m_tmp;
        //m_tmp = new COperatingBody(rhs.m_body_type);

        COperatingBody ob(*this);

        // volumes does not have to be zero
        assert (!is_float_equal(m_volume->si_value(), 0) &&
            !is_float_equal(rhs.m_volume->si_value(), 0));

        float n_t = (m_volume->si_value() * m_temperature->si_value() + rhs.m_volume->si_value() *
                    rhs.m_temperature->si_value()) / (m_volume->si_value() + rhs.m_volume->si_value());

        ob.m_temperature->set_si_value(n_t);
        *ob.m_volume += *rhs.m_volume;

        for (auto &local : *m_body_parameters)
        {
            auto alien = get_same_param(local, rhs.m_body_parameters);
            if (!alien)
            {
                CBodyParam l_m_tmp(*local);
                l_m_tmp.set_si_value(0);
                CBodyParam val = add(local, &l_m_tmp, rhs);
                ob.m_body_parameters->emplace_back(new CBodyParam(val));
            }
            else
            {
                CBodyParam val = add(local, alien, rhs);
                ob.m_body_parameters->emplace_back(new CBodyParam(val));
            }
        }

        fill_missing_items(&ob, &rhs);

        return ob;
    }

    COperatingBody & COperatingBody::operator+=(const COperatingBody &rhs)
    {
        if (is_float_equal(m_volume->si_value(), 0))
        {
            *this = rhs;
            return *this;
        }

        for (auto &local : *m_body_parameters)
        {
            auto alien = get_same_param(local, rhs.m_body_parameters);

            CBodyParam l_m_tmp(*local);
            l_m_tmp.set_si_value(0);

            alien = alien ? alien : &l_m_tmp;

            *local = add(local, alien, rhs);
        }

        fill_missing_items(this, &rhs);

        float n_t, n_p;

        n_t = m_temperature->si_value() > 0 ? m_temperature->si_value() : rhs.m_temperature->si_value();
        n_p = m_pressure->si_value() > 0 ? m_pressure->si_value() : rhs.m_pressure->si_value();

        if (m_volume->si_value() > 0 || rhs.m_volume->si_value() > 0)
        {
            n_t = (m_volume->si_value() * m_temperature->si_value() + rhs.m_volume->si_value() * rhs.m_temperature->si_value()) /
                  (m_volume->si_value() + rhs.m_volume->si_value());
            n_p = (m_volume->si_value() * m_pressure->si_value() + rhs.m_volume->si_value() * rhs.m_pressure->si_value()) /
                  (m_volume->si_value() + rhs.m_volume->si_value());
        }

        m_temperature->set_si_value(n_t);
        m_pressure->set_si_value(n_p);
        *m_volume += *rhs.m_volume;

        return *this;
    }

    CBodyParam *COperatingBody::get_temperature() const
    {
        return m_temperature;
    }

    CBodyParam *COperatingBody::get_pressure() const
    {
        return m_pressure;
    }

    void COperatingBody::set_si_temperature(const float &temper)
    {
        m_temperature->set_si_value(temper);
    }

    void COperatingBody::set_si_pressure(const float &pressure)
    {
        m_pressure->set_si_value(pressure);
    }


    CBodyParam *COperatingBody::get_same_param(CBodyParam *local, std::vector<CBodyParam *> *alien)
    {
        for (auto item : *alien)
        {
            if (item->is_acceptable(*local) && (item->additional() == local->additional()))
            {
                return item;
            }
        }

        return nullptr;
    }

    void COperatingBody::reset_body()
    {
        for (auto &item : *m_body_parameters)
        {
            delete item;
        }

        m_body_parameters->clear();
    }

    NCore::E_BODY_TYPE COperatingBody::body_type() const
    {
        return m_body_type;
    }

    void COperatingBody::fill_missing_items(COperatingBody *body0, const COperatingBody *body1)
    {
        for (auto &alien_param : *body1->m_body_parameters)
        {
            auto local_param = get_same_param(alien_param, body0->m_body_parameters);

            if (!local_param)
            {
                CBodyParam param(*alien_param);
                param.set_si_value(0);
                param = add(&param, alien_param, *body1);

                body0->m_body_parameters->emplace_back(new CBodyParam(param));
            }
        }
    }

    CBodyParam COperatingBody::add(CBodyParam *local, CBodyParam *alien, const COperatingBody &rhs)
    {
        CBodyParam t_local(*local);

        t_local.set_si_value( ( (local->si_value()) * (m_volume->si_value()) + (rhs.m_volume->si_value()) * (alien->si_value()))
            / ( (m_volume->si_value()) + (rhs.m_volume->si_value()) ) );

        return t_local;
    }

    float COperatingBody::get_si_temperature() const
    {
        return m_temperature->si_value();
    }

    float COperatingBody::get_si_pressure() const
    {
        return m_pressure->si_value();
    }

    CBodyParam *COperatingBody::get_volume() const
    {
        return m_volume;
    }

    float COperatingBody::get_si_volume() const
    {
        return m_volume->si_value();
    }

    void COperatingBody::set_si_volume(const float &volume)
    {
        m_volume->set_si_value(volume);
    }

    COperatingBody COperatingBody::operator-(const long double &volume)
    {
        //delete m_tmp;
        //m_tmp = new COperatingBody(*this);
        COperatingBody tmp(*this);

        long double left_volume = tmp.get_si_volume() - volume;

        long double m_tmp_vol = left_volume <= 0 ? m_volume->si_value() : volume;
        long double loc_vol = left_volume <= 0 ? 0 : left_volume;

        tmp.m_volume->set_si_value(static_cast<float>(std::round(m_tmp_vol)));
        m_volume->set_si_value(static_cast<float>(std::round(loc_vol)));

        return tmp;
    }

    CBodyParam *COperatingBody::add_parameter(CBodyParam *parameter)
    {
        m_body_parameters->push_back(parameter);
        return parameter;
    }

    CBodyParam *COperatingBody::get_parameter(const uint16_t &index)
    {
        if (index >= m_body_parameters->size())
        {
            return nullptr;
        }

        return m_body_parameters->at(index);
    }

    CBodyParam *COperatingBody::remove_parameter(const uint16_t &index)
    {
        if (index >= m_body_parameters->size())
        {
            return nullptr;
        }

        CBodyParam *measured_value = m_body_parameters->at(index);

        m_body_parameters->erase(m_body_parameters->begin() + index);

        return measured_value;
    }

    CBodyParam *COperatingBody::remove_parameter(CBodyParam *parameter)
    {
        uint16_t counter = 0;
        bool  is_deleted = false;

        for (auto &item : *m_body_parameters)
        {
            if (item == parameter)
            {
                m_body_parameters->erase(m_body_parameters->begin() + counter);
                is_deleted = true;
                break;
            }

            counter++;
        }

        return is_deleted ? parameter : nullptr;
    }

    uint16_t COperatingBody::parameters_count() const
    {
        return m_body_parameters->size();
    }

    COperatingBody COperatingBody::operator*(const float &value)
    {
        if (value < 0)
        {
            throw std::runtime_error("in 'COperatingBody COperatingBody::operator*(const float &value)' volume can't be less than zero");
        }

        COperatingBody body = *this;
        body.m_volume->set_si_value(body.m_volume->si_value() * value);

        return body;
    }

    COperatingBody &COperatingBody::operator*=(const float &value)
    {
        if (value < 0)
        {
            throw std::runtime_error("in 'COperatingBody COperatingBody::operator*(const float &value)' volume can't be less than zero");
        }

        m_volume->set_si_value(m_volume->si_value() * value);

        return *this;
    }

    bool COperatingBody::is_test() const
    {
        return m_is_test;
    }

    void COperatingBody::set_test(const bool &yes_no)
    {
        m_is_test = yes_no;
    }

    bool operator==(const COperatingBody &lhs, const COperatingBody &rhs)
    {
        if (lhs.m_body_parameters->size() != rhs.m_body_parameters->size())
        {
            return false;
        }

        for (auto &left_param : *lhs.m_body_parameters)
        {
            CBodyParam * right_param = COperatingBody::get_same_param(left_param, rhs.m_body_parameters);
            if (!right_param)
            {
                return false;
            }

            if (*left_param != *right_param)
            {
                return false;
            }
        }

        return true;
    }


}
