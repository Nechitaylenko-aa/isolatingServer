//
// Created by artem on 04.04.24.
//

#include <stdexcept>
#include "../include/CCap.h"
#include "../include/CCell.h"
#include "CContainer.h"

namespace NCore
{
    CCap::CCap(CCell *owner, const E_BODY_TYPE &body_type, const E_CAP_DIRECTION &external_direction,
               const E_CONTOUR_TYPE &contour_type)
    {
        m_owner = owner;

        // for debug purposes. owner is main cell
        /*if (!m_owner->get_owner())
            assert(m_owner->get_owner() == nullptr);
        else
            assert(m_owner->get_owner()->get_owner() == nullptr);*/

        m_body_type = body_type;
        m_external_direction = external_direction;
        m_internal_direction = m_external_direction == CD_INPUT ? CD_OUTPUT : CD_INPUT;
        m_contour = contour_type;

        m_operating_body = nullptr;
    }

    CCap::~CCap()
    {
        delete m_operating_body;
        delete m_finish_body;
    }

    void CCap::set_id(const Tuint64 &id)
    {
        m_id = id;
    }

    Tuint64 CCap::get_id() const
    {
        return m_id;
    }

    CCell *CCap::get_owner()
    {
        return m_owner;
    }

    E_BODY_TYPE CCap::get_body_type() const
    {
        return m_body_type;
    }

    void CCap::set_conductor(CConductor *conductor, CCell *inquirer)
    {
        if (!conductor) {
            m_external_conductor = nullptr;
            m_internal_conductor = nullptr;
            return;
        }

        CCell * curr = inquirer;

        while (curr != nullptr)
        {
            if (curr == m_owner)
            {
                break;
            }

            curr = curr->get_owner();
        }

        CConductor ** cond = curr == nullptr ? &m_external_conductor : &m_internal_conductor;

        *cond = conductor;
    }

    CConductor *CCap::get_conductor(CCell *inquirer)
    {
        CCell *curr = inquirer;
        while(curr)
        {
            if (curr == m_owner)
            {
                break;
            }
            curr = curr->get_owner();
        }
        CConductor *ret = curr == nullptr ? m_external_conductor : m_internal_conductor;

        return ret;
    }

    E_CONTOUR_TYPE CCap::get_contour() const
    {
        return m_contour;
    }

    E_CAP_DIRECTION CCap::get_direction(CCell *watcher)
    {
        if (watcher == m_owner)
        {
            return m_internal_direction;
        }

        CCell * current = watcher;

        while(current)
        {
            if (current == m_owner)
            {
                return m_internal_direction;
            }

            current = current->get_owner();
        }

        return m_external_direction;
    }

    COperatingBody *CCap::put_ob(const COperatingBody *op_b, CConductor *sender)
    {
        // ВСЕГДА ПРИШЕДШЕЕ ТЕЛО ДОЛЖНО СОВПАДАТЬ с нашей природой и создавать копию тела у себя и передавать копию дальше
        if (!set_ob(op_b))
        {
            return nullptr;
        }

        if (!m_internal_conductor && !m_external_conductor)
        {
            return nullptr;
        }
        //return nullptr;
        /*
         Эта кепка принадлежит:
         1. проектной клетке.
            а) внутренняя труба OUT
               -sender != nullptr && sender == m_external_conductor - значит тело пришло из более высокой иерархии
                без вариантов если можно - запустить тело по трубе m_internal_conductor
               - sender == nullptr - инъекция извне, что делать? ХЗ получается двузначность. недопустимо. принимаем - просто кладем OB для идентификации и никуда не пускаем
                 ДЛЯ ПУСКА ПО ТРУБАМ ЮЗАЙ INJECT !!!!!!!!!!!!!!!!!!!!!!
            б) внутренняя труба INPUT
               - sender == m_internal_conductor. Запустить калбэк для опроса серврера
                 передать тело наружу
               - sender == m_external_conductor - передаем на  m_internal_conductor
               - sender = nullptr. Просто возвращаем обратно.
        */
        if (m_owner->cell_role() == ECellRole::ECR_SUBPROJECT)
        {
            if (sender == m_external_conductor)
            {
                if (m_internal_conductor)
                {
                    return m_internal_conductor->put_ob(m_operating_body, this);
                }
                else
                {
                    return nullptr;
                }
            }
            else if (sender == m_internal_conductor)
            {
                // калбэк для обращения к серверу за оборудованием
                if (m_cbFlowComplete)
                {
                    m_cbFlowComplete(this);
                }

                if (m_external_conductor)
                {
                    return m_external_conductor->put_ob(m_operating_body, this);
                }
                else
                {
                    if (!m_finish_body)
                        m_finish_body = new COperatingBody(*m_operating_body);
                    else
                        *m_finish_body = *m_operating_body;

                    m_finish_body->set_si_volume(0);

                    return m_finish_body;
                }
            }
            else
            {
                return m_operating_body;
            }
        }
        else
        {
            if (sender == nullptr && m_external_conductor)
            {
                return m_external_conductor->put_ob(m_operating_body, this);
            }

            if (sender && sender == m_external_conductor)
            {
                return m_owner->put_ob(m_operating_body, this);
            }
        }

        throw std::runtime_error("Caps undefined behaviour");
    }

    COperatingBody *CCap::get_ob()
    {
        return m_operating_body;
    }

    COperatingBody *CCap::inject_ob(COperatingBody *ob)
    {
        if (!set_ob(ob))
        {
            return nullptr;
        }

        // if this cap is project cell's cap
        if (m_owner->cell_role() == ECellRole::ECR_SUBPROJECT)
        {
            if (m_internal_conductor)
            {
                return m_internal_conductor->put_ob(m_operating_body, this);
            }
            else
            {
                return m_owner->put_ob(ob, this);
            }
        }

        // make test -----------------------------------------------
        COperatingBody * result;

        auto test = new COperatingBody(*ob);
        test->set_test(true);

        do
        {
            result = m_internal_conductor->put_ob(test, this);
        } while (!result);

        delete test; //--------------------------------------------

        // Предполагается, что весовые коэффициенты везде стоят. Пускаем рабочее тело
        do
        {
            result = m_internal_conductor->put_ob(m_operating_body, this);
        } while (!result);

        *m_operating_body = *result;

        return m_operating_body;
    }

    void CCap::set_contour(const E_CONTOUR_TYPE &contour)
    {
        m_contour = contour;
    }

    void CCap::get_parameters(CContainer &container)
    {
        ser_contour = m_contour;
        ser_body_type = m_body_type;
        ser_extern_dir = m_external_direction;
        ser_intern_dir = m_internal_direction;
        ser_ext_tube_id = m_external_conductor ? m_external_conductor->get_id() : 0;
        ser_intern_tube_id = m_internal_conductor ? m_internal_conductor->get_id() : 0;
        ser_owner_id = m_owner->get_id();

        container.add_member(m_id, "cap_id");
        container.add_member(ser_contour);
        container.add_member(ser_body_type);
        container.add_member(ser_extern_dir);
        container.add_member(ser_intern_dir);
        container.add_member(ser_ext_tube_id);
        container.add_member(ser_intern_tube_id);
        container.add_member(ser_owner_id);
    }

    void CCap::set_parameters(const CContainer &container)
    {
        m_contour = (E_CONTOUR_TYPE)ser_contour;
        m_body_type = (E_BODY_TYPE)ser_body_type;
        m_external_direction = (E_CAP_DIRECTION)ser_extern_dir;
        m_internal_direction = (E_CAP_DIRECTION)ser_intern_dir;

        assert(m_owner->get_id() == ser_owner_id);
    }

    void CCap::set_callbackOnFlowComplete(std::function<void(NCore::CCap *)> handler)
    {
        m_cbFlowComplete = std::move(handler);
    }

    bool CCap::set_ob(const COperatingBody *p_body)
    {
        if (!p_body || m_body_type != p_body->body_type())
        {
            return false;
        }
        if (!m_operating_body)
        {
            m_operating_body = new COperatingBody(*p_body);
        }
        else
        {
            *m_operating_body = *p_body;
        }
        return true;
    }

    void CCap::set_inner_conductor(CConductor *conductor)
    {
        m_internal_conductor = conductor;
    }

    void CCap::set_outer_conductor(CConductor *conductor)
    {
        m_external_conductor = conductor;
    }
}
