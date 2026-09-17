// actuator/CInternalInstrument.cpp
#include "CInternalInstrument.h"
#include "NComponent.h"
#include "../CAutomationAtoms.h"

namespace NCore
{
    CInternalInstrument::CInternalInstrument(NComponent *owner, const SSignalRole &role)
            : Instrument(role), m_owner(owner){}

    NComponent *CInternalInstrument::owner() const
    {
        return m_owner;
    }

    const SSignalRole CInternalInstrument::role() const
    {

        return m_role;
    }

    float CInternalInstrument::value() const
    {
        return m_owner->read_state_signal(m_role, m_role.index);
    }

}
