//
// Created by artem on 04.04.24.
//

#include <stdexcept>
#include <algorithm>
#include "../include/CCell.h"
#include <unordered_set>
#include <iostream>
#include "NComponent.h"

//static bool headCreated = false; // for debug purpose

namespace NCore
{

    CCell::CCell(CCell *owner)
    {
        // create any case
        m_tubes     = new std::vector<CConductor*>();
        m_components= new std::vector<CCell*>();
        m_additional_caps = new std::vector<CCap*>();
        m_bodies_caps = new std::vector<SBodySet*>();
        m_ser_project_type = 0;
        m_owner_id = 0;

        m_owner = owner;

        m_info_bus = m_owner == nullptr ? new CInfoBus(this) : m_owner->m_info_bus;

        if (!owner)
        {
            m_id = 1;
            m_last_id = 1;
            p_last_id  = &m_last_id;
            m_cell_role = ECellRole::ECR_SUBPROJECT;
        }
        else
        {
            m_cell_role = ECellRole::ECR_COMPONENT;
            p_last_id = m_owner->p_last_id;
            m_id = ++(*p_last_id);
            m_last_id = 0;
            m_owner_id = m_owner->m_id;
            //assert(m_owner->m_owner == nullptr);
            auto res = owner->add_component(this);
            assert(res != nullptr);
            assert(m_id != 1);
        }
    }

    CCell::~CCell()
    {

        for (auto &set : *m_bodies_caps)
        {
            delete set->cap_in;
            delete set->cap_out;
            delete set;
        }
        m_bodies_caps->clear();

        for (auto &item : *m_additional_caps)
        {
            delete item;
        }
        m_additional_caps->clear();

        for (auto &item : *m_tubes)
        {
            delete item;
        }

        for (auto &item : *m_components)
        {
            delete item;
        }
        m_components->clear();

        /** !!! продумать когда встроим проект в другой!!! вода главной кепки принадлежит TOR */
        if (m_owner)
        {
            for (auto &body : m_bodies)
            {
                delete body;
            }
            m_bodies.clear();
        }


        delete m_bodies_caps;
        delete m_additional_caps;
        delete m_tubes;
        delete m_components;

        if (m_owner == nullptr)
        {
            delete m_info_bus;
        }
    }

    void CCell::setup_id(CConductor *cond)
    {
        if (cond)
        {
            cond->m_id = ++(*p_last_id);
            return;
        }



        for (auto &set : *m_bodies_caps)
        {
            if (set->cap_in && set->cap_in->get_id() == 0)
                set->cap_in->set_id(++(*p_last_id));
            if (set->cap_out && set->cap_out->get_id() == 0)
                set->cap_out->set_id(++(*p_last_id));
        }

        if (m_owner)
        {
            m_project_type = m_owner->m_project_type;
        }
    }

    CCell *CCell::get_owner()
    {
        return m_owner;
    }

    void CCell::set_owner(CCell *owner)
    {
        if (m_owner == nullptr)
        {
            m_owner = owner;
            m_id = ++m_owner->m_last_id;
        }
    }

    Tuint64 CCell::get_id() const
    {
        return m_id;
    }

    void CCell::set_project_type(const E_PROJECT_TYPE &project_type)
    {
        m_project_type = project_type;
    }

    E_PROJECT_TYPE CCell::get_project_type() const
    {
        return m_project_type;
    }

    TComponentType CCell::get_component_type() const
    {
        return m_successor_component_type;
    }

    SConnectingResult CCell::connect_caps(CCap *cap_src, CCap *cap_dst)
    {
        SConnectingResult result;
        /// check caps are correct
        if (cap_src->get_body_type() != cap_dst->get_body_type())
        {
            return result;
        }

        bool one_component = cap_dst->get_owner() == cap_src->get_owner();
        bool the_same = cap_src == cap_dst;

        if (the_same || one_component)
        {
            return result;
        }

        /// caps ok

        CConductor * result_tube;

        CConductor * tube_src = cap_src->get_conductor(this);
        CConductor * tube_dst = cap_dst->get_conductor(this);

        /// if tubes are nullptr just create the tube and connect caps
        if (!tube_dst && !tube_src)
        {
            result_tube = new CConductor(this);
            bool cap0 = result_tube->add_cap(cap_dst);
            bool cap1 = result_tube->add_cap(cap_src);

            if (!cap0 || !cap1)
            {
                throw std::runtime_error("CConductor *CCell::connect_caps error when tubes are null");
            }
        }

        /// if there is only one tube, then just add cap with empty tube
        if ((!tube_src && tube_dst) || (tube_src && !tube_dst))
        {
            result_tube = tube_src ? tube_src : tube_dst;
            CCap * cap = tube_src ? cap_dst : cap_src;

            if (!result_tube->add_cap(cap))
            {
                throw std::runtime_error("CConductor *CCell::connect_caps error when one tube is null");
            }
        }

        /// if there are both tubes, then all caps move from one tube to another and delete empty from owner
        if (tube_src && tube_dst)
        {
            tube_src->move_caps_to_tube(tube_dst);
            result_tube = tube_dst;

            result.trash = this->remove_tube(tube_src);
            assert(result.trash != nullptr);
        }

        result.result_conductor = result_tube;

        return result;
    }

    CConductor *CCell::remove_tube(CConductor *conductor)
    {
        if (!conductor)
        {
            return nullptr;
        }

        // release caps
        /* // do not release due the undo/redo
        for (auto &cap : *conductor->m_caps)
        {
            cap->set_conductor(nullptr, this);
        }*/

        int counter = 0;

        for (auto & m_tube : *m_tubes)
        {
            if (m_tube == conductor)
            {
                m_tubes->erase(m_tubes->begin() + counter);
                break;
            }
            counter++;
        }

        return conductor;
    }

    bool CCell::debug_check_id()
    {
        std::vector<Tuint64> ids;

        /// collect all id in to this vector

        ids.push_back(m_id);

        for (auto &item : *m_bodies_caps)
        {
            ids.push_back(item->cap_in->get_id());
            ids.push_back(item->cap_out->get_id());
        }

        for (auto &cap : *m_additional_caps)
        {
            ids.push_back(cap->get_id());
        }

        for (auto &comp : *m_components)
        {
            ids.push_back(comp->get_id());
            for (auto &item : *comp->m_bodies_caps)
            {
                ids.push_back(item->cap_in->get_id());
                ids.push_back(item->cap_out->get_id());
            }
        }

        for (auto &item : *m_tubes)
        {
            ids.push_back(item->get_id());
        }

        std::sort(ids.begin(), ids.end());

        bool res = std::adjacent_find(ids.begin(), ids.end()) == ids.end();

        return res;
    }

    void CCell::clean_cell()
    {
        this->clear_base_caps();
        for (auto &item : *m_additional_caps)
        {
            delete item;
        }
        m_additional_caps->clear();

        for (auto &item : *m_tubes)
        {
            delete item;
        }

        for (auto &item : *m_components)
        {
            delete item;
        }
        m_components->clear();
    }

    COperatingBody *CCell::put_ob(COperatingBody *body, CCap *sender)
    {
        /// we can accept OB from our caps only
        if (sender && !is_mine_cap(sender))
        {
            return nullptr;
        }

        COperatingBody * local_body = get_body_instance(sender->get_body_type());
        assert(local_body != nullptr);

        *local_body = *body;

        CCap *opposite_cap;

        if (sender)
        {
            E_CAP_DIRECTION opp_dir = sender->get_direction(this) == CD_INPUT ? CD_OUTPUT : CD_INPUT;
            opposite_cap = find_cap(sender->get_body_type(), opp_dir);
        }
        else {
            opposite_cap = find_cap(local_body->body_type(), CD_OUTPUT);
        }


        return opposite_cap->put_ob(local_body, nullptr);
    }

    CCap *CCell::find_cap(const E_BODY_TYPE &body_type, const E_CAP_DIRECTION &direction)
    {
        CCap * r_cap;

        for (auto &item : *m_bodies_caps)
        {
            if (m_bodies.at(item->body_id)->body_type() != body_type)
            {
                continue;
            }

            r_cap = item->cap_in->get_direction(this) == direction ? item->cap_in : item->cap_out;

            return r_cap;
        }

        return nullptr;
    }

    bool CCell::is_mine_cap(CCap *cap)
    {
        auto pred = [=](SBodySet * item){ return item->cap_in == cap || item->cap_out == cap; };
        return std::any_of(m_bodies_caps->begin(), m_bodies_caps->end(), pred);
    }

    COperatingBody *CCell::get_body_instance(const E_BODY_TYPE &type)
    {
        for (auto &item : m_bodies)
        {
            if (item->body_type() == type)
            {
                return item;
            }
        }

        return nullptr;
    }
/*
    CCap *CCell::get_cap_in(const E_BODY_TYPE &body_type, const E_CONTOUR_TYPE &contour)
    {
        for (auto &item : *m_bodies_caps)
        {
            if (item->body->body_type() != body_type)
            {
                continue;
            }

            return item->cap_in;
        }

        for (auto &cap : *m_additional_caps)
        {
            if (cap->get_direction(nullptr) == CD_INPUT)
            {
                return cap;
            }
        }

        return nullptr;
    }

    CCap *CCell::get_cap_out(const E_BODY_TYPE &body_type, const E_CONTOUR_TYPE &contour)
    {
        for (auto &item : *m_bodies_caps)
        {
            if (item->body->body_type() == body_type)
            {
                return item->cap_out;
            }
        }

        for (auto &cap : *m_additional_caps)
        {
            if (cap->get_direction(nullptr) == CD_OUTPUT)
            {
                return cap;
            }
        }

        return nullptr;
    }*/

    CCell *CCell::add_component(CCell *component)
    {
        assert(component != this);
        //assert(component->m_owner == this && this->m_owner == nullptr);

        bool is_found = std::find_if(m_components->begin(),
                                m_components->end(),
                                [component](const CCell* item){return item == component;}) != m_components->end();

        // на время охоты на ублюдков ситуация исключена. TODO: Не забыть убрать. Не убираем. Следим за нарушением логики
        assert(is_found == false);

        m_components->push_back(component);
        component->set_project_type(m_project_type);
        component->m_info_bus = m_info_bus;

        return m_components->back();
    }

    CConductor *CCell::add_tube(CConductor *conductor)
    {
        if (conductor->get_owner() != this)
            return nullptr;

        if (!std::any_of(m_tubes->begin(), m_tubes->end(), [conductor](const CConductor* item){return item==conductor;}))
            m_tubes->push_back(conductor);
        return conductor;
    }

    std::vector<NCore::CCell *> *CCell::get_components()
    {
        return m_components;
    }

    std::vector<NCore::CConductor *> *CCell::get_tubes()
    {
        return m_tubes;
    }

    void CCell::clear_cell()
    {
        for (auto &set : *m_bodies_caps)
        {
            delete set->cap_in;
            delete set->cap_out;
            //delete set->body;
            delete set;
        }
        m_bodies_caps->clear();

        for (auto &tube : *m_tubes)
        {
            delete tube;
        }
        m_tubes->clear();
    }

    /*void CCell::complete_deserialization()
    {
        for (auto &tube : *m_tubes)
        {
            auto connected_caps_ids = tube->connected_caps_id();

            for (auto &id : connected_caps_ids)
            {
                CCap *cap = get_cap_by_id(id);

                if (!tube->add_cap(cap))
                {
                    throw std::runtime_error("something wrong in 'CCell::complete_deserialization");
                }
            }

            tube->complete_deserialization();
        }
    }*/

    CCap *CCell::get_cap_by_id(const Tsize &id)
    {
        for (auto &cap : *m_additional_caps)
        {
            if (cap->get_id() == id)
            {
                return cap;
            }
        }

        for (auto &set : *m_bodies_caps)
        {
            if (set->cap_in->get_id() == id)
            {
                return set->cap_in;
            }
            if (set->cap_out->get_id() == id)
            {
                return set->cap_out;
            }
        }

        for (auto &comp : *m_components)
        {
            auto cap = comp->get_cap_by_id(id);

            if (cap)
            {
                return cap;
            }
        }

        return nullptr;
    }

    std::tuple<CCap*, CCap*, COperatingBody*> CCell::add_ob(COperatingBody *alien_ob, E_CONTOUR_TYPE contour)
    {
        assert(alien_ob != nullptr);
        auto ob_set = new SBodySet();

        ob_set->cap_in = new CCap(this, alien_ob->body_type(), CD_INPUT, contour);
        ob_set->cap_out = new CCap(this, alien_ob->body_type(), CD_OUTPUT, contour);

        COperatingBody * localOb = nullptr; //new COperatingBody(*alien_ob);

        auto it = std::find_if(m_bodies.begin(), m_bodies.end(), [alien_ob](const COperatingBody*item){
                               return *item == *alien_ob; });
        if (it != m_bodies.end())
        {
            ob_set->body_id = std::distance(m_bodies.begin(), it);
            localOb = *(it);
        } else
        {
            localOb = new COperatingBody(*alien_ob);
            ob_set->body_id = static_cast<long>(m_bodies.size());
            m_bodies.push_back(localOb);
        }

        if (!m_owner)
        {
            ob_set->cap_in->put_ob(localOb, nullptr);
        }

        ob_set->cap_in->set_id(++(*p_last_id));
        ob_set->cap_out->set_id(++(*p_last_id));

        m_bodies_caps->push_back(ob_set);

        return {ob_set->cap_in, ob_set->cap_out, m_bodies.at(ob_set->body_id)};
    }

    CCap *CCell::remove_input(CCap *input)
    {
        for (auto &set : *m_bodies_caps)
        {
            if (set->cap_in == input)
            {
                remove_from_tube(set->cap_in);
                set->cap_in = nullptr;
                return input;
            }
        }
        return nullptr;
    }

    CCap *CCell::remove_output(CCap *output)
    {
        for (auto &set : *m_bodies_caps)
        {
            if (set->cap_out == output)
            {
                remove_from_tube(set->cap_out);
                set->cap_out = nullptr;
                return output;
            }
        }
        return nullptr;
    }

    CCell *CCell::remove_component(CCell *comp)
    {
        int counter = 0;

        for (auto &item : *m_components)
        {
            if (item == comp)
            {
                m_components->erase(m_components->begin() + counter);
                return comp;
            }
            counter++;
        }
        return nullptr;
    }

    std::vector <CCap *> CCell::inputs()
    {
        auto arr = std::vector<CCap *>();
        for (auto &set : *m_bodies_caps)
        {
            arr.push_back(set->cap_in);
        }
        return arr;
    }

    std::vector <CCap *> CCell::outputs()
    {
        auto arr = std::vector<CCap *>();

        for (auto &set : *m_bodies_caps)
        {
            arr.push_back(set->cap_out);
        }
        return arr;
    }

    bool NCore::CCell::set_parameters(CContainer &container)
    {
        m_project_type = static_cast<E_PROJECT_TYPE>(m_ser_project_type);

        for (auto & set : * m_bodies_caps)
        {
            set->cap_in->set_parameters(container);
            set->cap_out->set_parameters(container);

            if (set->cap_in->get_body_type() == BT_UNDEF)
            {
                delete set->cap_in;
                set->cap_in = nullptr;
            }
            if (set->cap_out->get_body_type() == BT_UNDEF)
            {
                delete set->cap_out;
                set->cap_out = nullptr;
            }
        }

        if (!m_owner && m_owner_id > 0)
            return false;
        return true;
    }

    void NCore::CCell::get_parameters(CContainer &container)
    {
        container.add_member(m_id, "id");
        container.add_member(m_owner_id, "owner_id");
        container.add_member(m_last_id, "last_id");
        m_ser_project_type = m_project_type;
        container.add_member(m_ser_project_type, "project_type");

        if (!container.is_deserialize())
        {
            serialize_caps(container);
        }
        else
        {
            for (auto &body : m_bodies)
                delete body;
            m_bodies.clear();

            deserialize_caps(container);
        }
    }

/*
    std::pair<CCap *, CCap *> NCore::CCell::get_base_body_caps()
    {
        E_BODY_TYPE type = subprojects_base_body(m_project_type);

        for (auto &set : *m_bodies_caps)
        {
            switch (type)
            {
                case E_BODY_TYPE::BT_WATER:
                    if (m_bodies.at(set->body_id)->body_type() == E_BODY_TYPE::BT_WATER)
                        return {set->cap_in, set->cap_out};
                    continue;
                case E_BODY_TYPE::BT_GAS:
                    if (set->cap_in->get_body_type() == E_BODY_TYPE::BT_GAS)
                        return {set->cap_in, set->cap_out};
                    continue;
                case E_BODY_TYPE::BT_ELECTRICITY:
                    if (set->cap_in->get_body_type() == E_BODY_TYPE::BT_ELECTRICITY)
                        return {set->cap_in, set->cap_out};
                    continue;
                default:
                    continue;
            }
        }

        return {nullptr, nullptr};
    }*/

    std::vector<CCap *> CCell::get_base_inputs()
    {
        std::vector<CCap*> result;
        E_BODY_TYPE type = subprojects_base_body(m_project_type);

        for (auto &set : *m_bodies_caps)
        {
            auto input = set->cap_in;
            if (!input || input->get_body_type() != type)
            {
                continue;
            }
            if (input->get_contour() == CT_MAIN)
            {
                result.push_back(input);
            }
        }
        return result;
    }

    std::vector<CCap *> CCell::get_base_outputs()
    {
        std::vector<CCap*> result;
        E_BODY_TYPE type = subprojects_base_body(m_project_type);

        for (auto &set : *m_bodies_caps)
        {
            auto cap_out = set->cap_out;
            if (!cap_out || cap_out->get_body_type() != type)
            {
                continue;
            }
            if (cap_out->get_contour() == CT_MAIN)
            {
                result.push_back(cap_out);
            }
        }
        return result;
    }

    std::vector<CCap *> CCell::get_base_caps()
    {
        std::vector<CCap*> result;
        E_BODY_TYPE type = subprojects_base_body(m_project_type);

        for (auto &set : *m_bodies_caps)
        {
            auto cap_in = set->cap_in;
            auto cap_out = set->cap_out;

            if (cap_in && cap_in->get_body_type() == type)
            {
                if (cap_in->get_contour() == E_CONTOUR_TYPE::CT_MAIN)
                {
                    result.push_back(cap_in);
                }
            }

            if (cap_out && cap_out->get_body_type() == type)
            {
                if (cap_out->get_contour() == CT_MAIN)
                {
                    result.push_back(cap_out);
                }
            }
        }

        return result;
    }

    void CCell::clear_base_caps()
    {
        std::unordered_set<COperatingBody*> trash;

        for (auto &item : *m_bodies_caps)
        {
            if (item->cap_in)
            {
                remove_from_tube(item->cap_in);
            }

            if (item->cap_out)
            {
                remove_from_tube(item->cap_out);
            }

            delete item->cap_in;
            delete item->cap_out;

            if (m_owner)
            {
                auto ob = m_bodies.at(item->body_id);
                trash.insert(ob);
            }
            delete item;
        }

        m_bodies_caps->clear();

        m_bodies.erase( std::remove_if(m_bodies.begin(), m_bodies.end(),
                                       [&](COperatingBody* body)
                                       {
                                           if (trash.count(body))
                                           {
                                               delete body;
                                               return true;
                                           }
                                           return false;
                                       }),
                        m_bodies.end()
        );
    }

    COperatingBody *CCell::get_base_ob()
    {
        E_BODY_TYPE type = subprojects_base_body(m_project_type);
        for (auto &item : m_bodies)
        {
            if (item->body_type() == type)
            {
                return item;
            }
        }
        return nullptr;
    }

    void CCell::serialize_caps(CContainer &container)
    {
        // собираем инфу
        m_caps_amount = m_bodies_caps->size();
        container.add_member(m_caps_amount, "variadic");

        if (m_bodies_caps->empty())
        {
            return;
        }

        // создаем муляжи для сериализации
        CCap capin(this, BT_UNDEF, E_CAP_DIRECTION::CD_INPUT, E_CONTOUR_TYPE::CT_MAIN);
        CCap capout(this, BT_UNDEF, E_CAP_DIRECTION::CD_OUTPUT, E_CONTOUR_TYPE::CT_MAIN);
        bool in_used  =false;
        bool out_used = false;

        // отсутствующие кепки заменяем муляжами с BT_UNDEF что бы сериализовать всё
        for (auto &set : *m_bodies_caps)
        {
            if (!set->cap_in)
            {
                set->cap_in = &capin;
                in_used = true;
            }
            if (!set->cap_out)
            {
                set->cap_out = &capout;
                out_used = true;
            }

            //serialize caps pair
            set->cap_in->get_parameters(container);
            set->cap_out->get_parameters(container);
            container.add_member(set->body_id);
        }

        //Теперь убираем муляжи, нам еще работать
        if (in_used || out_used)
        {
            for (auto & set : * m_bodies_caps)
            {
                if (set->cap_in->get_body_type() == BT_UNDEF)
                {
                    set->cap_in = nullptr;
                }
                if (set->cap_out->get_body_type() == BT_UNDEF)
                {
                    set->cap_out = nullptr;
                }
            }
        }

        // delete capin;
        // delete capout;
    }

    void CCell::deserialize_caps(CContainer &container)
    {
        container.add_member(m_caps_amount, "variadic");
        container.set_total_times(1);

        if (!container.is_contains_variadik())
        {
            container.set_variadik(true);
            return;
        }

        // сначала очистим то что есть
        for (auto & set : * m_bodies_caps)
        {
            delete set->cap_in;
            delete set->cap_out;
            delete set;
        }
        m_bodies_caps->clear();

        // это уже второй раз и мы должны знать количество m_body_caps - m_caps_amount
        for (uint8_t idx = 0; idx < m_caps_amount; idx++)
        {
            auto *set = new SBodySet();

            set->cap_in = new CCap(this, BT_UNDEF, E_CAP_DIRECTION::CD_INPUT, E_CONTOUR_TYPE::CT_MAIN);
            set->cap_out = new CCap(this, BT_UNDEF, E_CAP_DIRECTION::CD_OUTPUT, E_CONTOUR_TYPE::CT_MAIN);

            m_bodies_caps->push_back(set);

            set->cap_in->get_parameters(container);
            set->cap_out->get_parameters(container);
            container.add_member(set->body_id);
        }
        container.set_current_time(1);
    }

    void CCell::remove_from_tube(CCap *cap)
    {
        auto tube = cap->get_conductor(this);
        if (!tube)
        {
            return;
        }

        if (tube->caps_amount() <= 2)
        {
            tube->clear_caps();
            delete remove_tube(tube);
            return;
        }

        tube->remove_cap(cap);
    }


    void CCell::printCell(int depth /* = 0 */)
    {
        // Отступ для вложенных компонентов
        std::string indent(depth * 2, ' ');

        // Тип клетки
        std::string cellType = (m_owner != nullptr) ? "Component" : "Project";

        std::cout << indent << "├─ " << cellType << " [ID: " << m_id << "]";

        // Если это компонент, показываем его тип
        if (m_id > 1)
        {
            auto add = m_owner ? "Component" : "Project cell";
            auto comp = dynamic_cast<NComponent*>(this);
            auto addSch = comp->schematicName();
            std::cout << " Type: " << add << " " << addSch;
        }
        std::cout << std::endl;

        // Печатаем наконечники (входы и выходы)
        printCaps(indent + "│  ");

        // Печатаем трубы, связанные с этой клеткой
        printTubes(indent + "│  ");

        // Рекурсивно печатаем дочерние компоненты
        if (m_components && !m_components->empty())
        {
            std::cout << indent << "│  └─ Children components:" << std::endl;
            for (auto* component : *m_components)
            {
                if (component)
                {
                    component->printCell(depth + 1);
                }
            }
        }
    }

    void CCell::printCaps(const std::string& indent)
    {
        if ((!m_bodies_caps || m_bodies_caps->empty()) &&
            (!m_additional_caps || m_additional_caps->empty()))
        {
            return;
        }

        std::cout << indent << "├─ Caps:" << std::endl;

        if (m_bodies_caps)
        {
            for (auto* bodySet : *m_bodies_caps)
            {
                if (bodySet)
                {
                    if (bodySet->cap_in)
                    {
                        printCapInfo(bodySet->cap_in, indent + "│  ", "IN");
                    }
                    if (bodySet->cap_out)
                    {
                        printCapInfo(bodySet->cap_out, indent + "│  ", "OUT");
                    }
                }
            }
        }

        // Печатаем дополнительные наконечники
        if (m_additional_caps)
        {
            for (auto* cap : *m_additional_caps)
            {
                if (cap)
                {
                    printCapInfo(cap, indent + "│  ", "ADD");
                }
            }
        }
    }

    void CCell::printCapInfo(CCap* cap, const std::string& indent, const std::string& Direction)
    {
        if (!cap) return;


        E_CAP_DIRECTION dir = m_owner ? cap->get_direction(m_owner) : cap->get_direction(this);
        std::string direction;
        switch (dir)
        {
            case CD_INPUT:
                direction = "IN";
                break;
            default:
                direction = "OUT";
                break;
        }

        std::string contour;
        switch (cap->get_contour())
        {
            case CT_MAIN: contour = "MAIN"; break;
            default: contour = "Add";
        }

        std::string bodyType;
        switch (cap->get_body_type())
        {
            case BT_WATER: bodyType = "Water"; break;
            case BT_ELECTRICITY: bodyType = "Light"; break;
            case BT_GAS: bodyType = "Gas"; break;
            default: bodyType = "Unknown";
        }

        CCell *owner = m_owner == nullptr ? this : m_owner;

        CConductor* conductor = cap->get_conductor(owner);
        std::string conductorInfo = conductor ?
                                    " -> Tube[ID:" + std::to_string(conductor->get_id()) + "]" : " (disconnected)";

        std::cout << indent << "├─ Cap[ID:" << cap->get_id()
                  << ", Dir:" << direction
                  << ", Contour:" << contour
                  << ", Body:" << bodyType << "]"
                  << conductorInfo << std::endl;
    }

    void CCell::printTubes(const std::string& indent)
    {
        if (!m_tubes || m_tubes->empty())
        {
            return;
        }

        std::cout << indent << "├─ Tubes:" << std::endl;

        for (auto* tube : *m_tubes)
        {
            if (tube)
            {
                tube->print(indent + "│  ");
            }
        }
    }

    void CCell::loadingUpdateOperationBody(COperatingBody *ob, uint ins, uint outs)
    {
        assert(m_bodies.empty() == true);
        assert(ob != nullptr);

        m_bodies.push_back(ob);

        for (auto &set : *m_bodies_caps)
        {
            if (set->cap_in)
            {
                set->cap_in->put_ob(ob, nullptr);
            }
            set->body_id = 0;
        }
    }

    Tstring CCell::get_description() const
    {
        if (!m_owner)
            return "Project cell";
        return "Component";
    }

    CInfoBus *CCell::info_bus()
    {
        return m_info_bus;
    }

    void CCell::clear_base_connections()
    {
        for (auto &unit : *m_bodies_caps)
        {
            unit->cap_in->set_inner_conductor(nullptr);
            unit->cap_out->set_inner_conductor(nullptr);
        }
    }
}
