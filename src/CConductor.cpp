//
// Created by artem on 04.04.24.
//

#include <algorithm>
#include "../include/CConductor.h"
#include "../include/CCell.h"
#include <iostream>

namespace NCore
{


    CConductor::CConductor(CCell *owner)
    {
        m_owner     = owner;
        m_body_type = E_BODY_TYPE::BT_UNDEF;

        m_caps          = new std::vector<CCap*>();


        m_owner->setup_id(this);

        // in the last queue
        m_calculator = new CConductCalculator(this);
        m_owner->add_tube(this);
    }

    CConductor::~CConductor()
    {
        delete m_calculator;
        delete m_caps;
    }

    Tuint64 CConductor::get_id() const
    {
        return m_id;
    }

    bool CConductor::is_existing_cap(CCap *cap)
    {
        return std::find(m_caps->begin(), m_caps->end(), cap) != m_caps->end();
    }

    bool CConductor::add_cap(CCap *cap)
    {
        if (!cap)
        {
            return false;
        }

        if (m_caps->empty())
        {
            m_body_type = cap->get_body_type();
        }

        if (cap->get_body_type() != m_body_type)
        {
            return false;
        }

        if (!is_existing_cap(cap))
        {
            m_caps->push_back(cap);
        }

        cap->set_conductor(this, m_owner);

        m_calculator->add_cap(cap);

        return true;
    }

    bool CConductor::check_correct()
    {
        int is_in{0}, is_out{0};

        for (auto &cap : *m_caps)
        {
            E_CAP_DIRECTION dir = cap->get_direction(m_owner);
            is_in += dir == CD_INPUT ? 1 : 0;
            is_out += dir == CD_OUTPUT? 1 : 0;
        }

        return is_in && is_out;
    }

    CCap *CConductor::remove_cap(CCap *cap)
    {
        assert(is_existing_cap(cap) == true);

        for (int i = 0; i < m_caps->size(); i++)
        {
            NCore::CCap *item = m_caps->at(i);

            if (item == cap)
            {

                /// если удаляется входная (для трубы) кепка, просто удаляем один флаг, но теперь нужен новый проход,
                /// т.к. баланс изменился. Обдумать.

                m_calculator->remove_cap(cap);
                m_caps->erase(m_caps->begin() + i);

                break;
            }
        }

        return cap;
    }

    void CConductor::move_caps_to_tube(CConductor *dst_tube)
    {
        auto alien_v = dst_tube->m_caps;    // вектор чужих кепок
        auto local_v = m_caps;              // вектор локальных кепок

        for (auto &cap : *m_caps)
        {
            cap->set_conductor(dst_tube, dst_tube->m_owner);
        }

        for (auto &cap : *m_caps)
        {
            m_calculator->remove_cap(cap);
            cap->set_conductor(dst_tube, dst_tube->get_owner());
            dst_tube->m_calculator->add_cap(cap);
        }

        alien_v->insert(alien_v->end(), std::make_move_iterator(local_v->begin()),
                                        std::make_move_iterator(local_v->end()));

        m_caps->clear();
        m_calculator->clear();
        //m_body_type = E_BODY_TYPE::BT_UNDEF;
    }

    CCell *CConductor::get_owner()
    {
        return m_owner;
    }

    COperatingBody *CConductor::put_ob(const COperatingBody *ob, CCap *sender)
    {
        return m_calculator->put_ob(ob, sender);
    }

    bool CConductor::is_connection_ok() const
    {
        int in{0}, out{0};
        for (auto &cap : *m_caps)
        {
            in  += cap->get_direction(m_owner) == CD_INPUT ? 1 : 0;
            out += cap->get_direction(m_owner) == CD_OUTPUT? 1 : 0;
        }

        return in && out;
    }

    std::vector<Tsize> CConductor::connected_caps_id()
    {
        if (!m_caps->empty())
        {
            std::vector<Tsize> ids;
            for (auto &cap : *m_caps)
            {
                ids.push_back(cap->get_id());
            }

            return ids;
        }

        return d_caps_id;
    }

    uint16_t CConductor::caps_amount() const
    {
        return m_caps->size();
    }

    CCap *CConductor::cap(const uint16_t &index)
    {
        if (index >= m_caps->size())
            return nullptr;
        return m_caps->at(index);
    }

    void CConductor::clear_caps()
    {
        for (auto &cap : *m_caps)
        {
            cap->set_conductor(nullptr, m_owner);
        }
        m_caps->clear();
    }

    // В файле CConductor.cpp
    void CConductor::print(const std::string& indent)
    {
        std::cout << indent << "├─ Tube [ID:" << m_id
                  << ", Caps:" << caps_amount()
                  << ", Status:" << (is_connection_ok() ? "OK" : "INCOMPLETE") << "]"
                  << std::endl;

        if (m_caps && !m_caps->empty())
        {
            std::string subIndent = indent + "│  ";
            std::cout << subIndent << "└─ Connected to:" << std::endl;

            for (auto* cap : *m_caps)
            {
                if (cap && cap->get_owner())
                {
                    CCell* owner = cap->get_owner();
                    std::string ownerType = (owner->get_project_type() == pt_undef) ?
                                            "Component" : "Project";

                    std::cout << subIndent << "   ├─ " << ownerType
                              << " [ID:" << owner->get_id()
                              << "] -> Cap[ID:" << cap->get_id() << "]" << std::endl;
                }
            }
        }
    }
}
