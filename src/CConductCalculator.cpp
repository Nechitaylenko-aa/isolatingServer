//
// Created by artem on 30.06.24.
//

#include "../include/CConductCalculator.h"
#include "../include/COperatingBody.h"
#include "../include/CCap.h"
#include "../include/CConductor.h"


namespace NCore
{

    NCore::CConductCalculator::CConductCalculator(NCore::CConductor *tube)
    {
        m_conductor = tube;
        m_caps_in_state = new std::map<CCap*, s_cap_data>();
        m_weight_map = new std::map<CCap*, float>();
    }

    NCore::CConductCalculator::~CConductCalculator()
    {
        delete m_caps_in_state;
        delete m_weight_map;
        delete tmp_body;
    }

    COperatingBody *CConductCalculator::calculate_weights(const COperatingBody *p_body, CCap *sender)
    {
        float test_vol = 1000000; // это считается очень много рабочего тела

        if (!tmp_body)
        {
            tmp_body = new COperatingBody(m_conductor->m_body_type);
        }


        COperatingBody test_body(*p_body);
        test_body.set_si_volume(test_vol);

        bool is_ok = true;
        long double sum = 0;

        for (auto &pair : *m_weight_map)
        {
            /// засунем дофига воды и посмотрим сколько влезло
            auto *tmp = pair.first->put_ob(&test_body, m_conductor);

            if (!tmp)
            {
                pair.second = 0;
                is_ok = false;
                continue;
            }

            auto gone = test_vol - tmp->get_si_volume();
            pair.second =  static_cast<float>(gone);
            sum += gone;
        }

        if (!is_ok)
        {
            return nullptr;
        }

        for (auto &pair : *m_weight_map)
        {
            pair.second = static_cast<float>(pair.second / sum);
        }

        *tmp_body = test_body;

        return tmp_body;
    }

    bool CConductCalculator::check_tube_income(const COperatingBody *ob, CCap *sender)
    {
        if (!m_conductor->is_existing_cap(sender))
        {
            return false;
        }

        if (ob->body_type() != m_conductor->m_body_type)
        {
            return false;
        }
        /// пока неясно ошибка ли это, но это ненормально, если входящий поток с нулевым объёмом
        if (NCore::is_float_equal(ob->get_si_volume(), 0))
        {
            return false;
        }

        m_caps_in_state->find(sender)->second.is_ob = true;
        long double  sum_in = 0;

        for (auto &pair : *m_caps_in_state)
        {
            if (!pair.second.is_ob)
            {
                return false;
            }

            sum_in += pair.first->get_ob()->get_si_volume();
        }

        /// подсчёт весовых коэффициентов входящих (для трубы) кепок
        if (!m_income_weights_complete)
        {
            for (auto &item: *m_caps_in_state)
            {
                item.second.weight = static_cast<float>(item.first->get_ob()->get_si_volume() / sum_in);
            }

            m_income_weights_complete = true;
        }

        return true;
    }

    void CConductCalculator::reset_caps_state()
    {
        for (auto &i : *m_caps_in_state)
        {
            i.second.is_ob = false;
        }
    }

    void CConductCalculator::reset_caps_weights()
    {
        for (auto &pair : *m_weight_map)
        {
            pair.second = 1.0;
        }
    }

    std::map<CCap *, s_cap_data> * CConductCalculator::CConductCalculator::caps_in_state()
    {
        return m_caps_in_state;
    }

    std::map<CCap *, float> * CConductCalculator::CConductCalculator::weights_map()
    {
        return m_weight_map;
    }

    void CConductCalculator::remove_cap(CCap *cap)
    {
        if (cap->get_direction(m_conductor->m_owner) == CD_OUTPUT)
        {
            m_caps_in_state->erase(m_caps_in_state->find(cap));
            reset_caps_state();
        }
        else
        {
            m_weight_map->erase(m_weight_map->find(cap));
            reset_caps_weights();
        }
    }

    void CConductCalculator::clear()
    {
        m_caps_in_state->clear();
        m_weight_map->clear();
        if (tmp_body)
        {
            tmp_body->reset_body();
        }
    }

    COperatingBody *CConductCalculator::put_ob(const COperatingBody *ob, CCap *sender)
    {
        if (ob->is_test())
        {
            return calculate_weights(ob, sender);
        }

        /// если не все кепки дали "сок", то возвращаем nullptr
        if (!check_tube_income(ob, sender))
        {
            return nullptr;
        }

        if (!tmp_body)
        {
            tmp_body = new COperatingBody(m_conductor->m_body_type);
        }

        /// и так все кепки дали РТ и суммируем то что дали
        COperatingBody income(m_conductor->m_body_type);
        COperatingBody drop(m_conductor->m_body_type);

        for (auto &item : *m_conductor->m_caps)
        {
            if (item->get_direction(m_conductor->m_owner) == CD_INPUT)
            {
                continue;
            }

            income += *item->get_ob();
        }

        bool    is_complete = true;

        /// разделить РТ на исходящие (для трубы) кепки согласно весовым коэффициентам и суммировать возврат и вернуть
        for (auto &pair : *m_weight_map)
        {
            /// вот эту каплю с весовым коэффициентом мы отправим в текущую кепку
            drop = income * pair.second;

            COperatingBody *tb = pair.first->put_ob(&drop, m_conductor);
            if (!tb)
            {
                is_complete = false;
            }
            else
            {
                *tmp_body += *tb;
            }
        }

        /// если не все кепки дали feedback, то возвращаем nullptr
        if (!is_complete)
        {
            return nullptr;
        }

        /// весовой коэффициент этой ветки ДО нас подсчитан и возвращаем в объёме согласно весовому коэффициенту.
        *tmp_body *= m_caps_in_state->find(sender)->second.weight;

        return tmp_body;
    }

    void CConductCalculator::add_cap(CCap *cap)
    {
        /// тут очень важно понимать, что выходные кепки для трубы становятся входными, через которые труба получает РТ,
        /// иначе эта кепка выходная для трубы и у нее должен быть весовой коэфф
        if (cap->get_direction(m_conductor->m_owner) == CD_OUTPUT)
        {
            m_caps_in_state->emplace(cap, s_cap_data());
        }
        else
        {
            m_weight_map->emplace(cap, 1.0);
        }
    }

}
