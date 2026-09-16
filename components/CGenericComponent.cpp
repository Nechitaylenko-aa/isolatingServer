//
// Created by artem on 24.08.26.
//

#include "CGenericComponent.h"
#include "../automation/CAutomationAtoms.h"

namespace NCore
{
    CGenericComponent::CGenericComponent(IGeneralTor *tor, CCell *owner, TComponentType type, const SCaps &caps)
            : NComponent(tor, owner, type)
    {
        m_descript = "Generic";

        auto bt = (E_BODY_TYPE)type.index();
        assert(bt < BT_UNDEF);

        for (auto & contour : caps.contours_in)
        {
            COperatingBody ob(bt);
            auto [in, out, body] = add_ob(&ob, contour);
            m_inputs.push_back(in);
            delete this->remove_output(out); // нужен только вход в этой паре
        }
        for (auto & contour : caps.contours_out)
        {
            COperatingBody ob(BT_WATER);
            auto [in, out, body] = add_ob(&ob, contour);
            m_outputs.push_back(out);
            delete this->remove_input(in);
        }
        // тело для put_ob — берём из первой пары, если есть; иначе можно завести
        // отдельный m_body как у CLightFilter при желании подмешивать эффект позже
    }

    COperatingBody *CGenericComponent::put_ob(COperatingBody *body, CCap *sender)
    {
        if (sender == m_inputs.at(0))
        {
            return m_outputs.at(0)->put_ob(body, nullptr);
        }
        return nullptr;
    }

    std::vector<SSignalRole> CGenericComponent::required_signals() const
    {
        return m_signals;
    }

    std::vector<SCommandRole> CGenericComponent::required_commands() const
    {
        return m_commands;
    }

    std::vector<SAutomationSpec> CGenericComponent::automation() const
    {
        return {};
    }

    std::vector<SDesignConstraintSpec> CGenericComponent::design_constraints() const
    {
        return {};
    }
}
