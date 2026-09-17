//
// Created for automation layer-4.
//

#include "Actuator.h"
#include "NComponent.h"

namespace NCore
{
    Actuator::Actuator(CCap *anchor, const SCommandRole &role)
        : m_anchor(anchor)
        , m_role(role)
    {
        assert(m_anchor && "Actuator requires a resolved, non-null anchor Cap");
        Tstring tag = anchor->get_owner()->get_description() + " id_obj:" + std::to_string(anchor->get_owner()->get_id());
        m_tag = tag;
    }

    CCap *Actuator::anchor() const
    {
        return m_anchor;
    }

    NComponent *Actuator::owner() const
    {
        return dynamic_cast<NComponent*>(m_anchor->get_owner());
    }

    /*
    CInfoBus *Actuator::info_bus() const
    {
        auto *component = owner();
        return component ? component->info_bus() : nullptr;
    }*/

    const SCommandRole &Actuator::role() const
    {
        return m_role;
    }

    void Actuator::set_tag(const Tstring &tag)
    {
        m_tag = tag;
    }

    Tstring Actuator::tag() const
    {
        return m_tag;
    }

    void Actuator::set_io_address(const Tstring &addr)
    {
        m_io_address = addr;
    }

    Tstring Actuator::io_address() const
    {
        return m_io_address;
    }
}
