//
// Created by artem on 24.08.26.
//

#ifndef NYM_PROJECT_CGENERICCOMPONENT_H
#define NYM_PROJECT_CGENERICCOMPONENT_H

#include "../include/NComponent.h"

// CGenericComponent.h
namespace NCore
{
    struct SCaps { std::vector<E_CONTOUR_TYPE> contours_in; std::vector<E_CONTOUR_TYPE> contours_out; };

    class CGenericComponent : public NComponent
    {
    public:
        CGenericComponent(IGeneralTor *tor, CCell *owner, TComponentType type, const SCaps &caps);
        ~CGenericComponent() override = default;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;
        bool     set_parameters(CContainer &container) override { return true; }
        void     get_parameters(CContainer & container) override {}
        [[nodiscard]] Tstring  get_description() const override { return m_descript; }

    protected:
        void set_equipment(equip::CEquipment *equip) override {}
        void set_equipmentProxy(std::vector<SEquipLight> && items) override {}

        [[nodiscard]] std::vector<SSignalRole>           required_signals()      const override;
        [[nodiscard]] std::vector<SCommandRole>          required_commands()     const override;
        [[nodiscard]] std::vector<SAutomationSpec>       automation()             const override;
        [[nodiscard]] std::vector<SDesignConstraintSpec> design_constraints()     const override;


    public:
        /*void set_required_signals(std::vector<SSignalRole> s)  { m_signals = std::move(s); }
        void set_required_commands(std::vector<SCommandRole> c) { m_commands = std::move(c); }
        void set_internal_variables(std::vector<SInternalVariable> v) {m_variables = std::move(v);}*/

    private:
        std::vector<SSignalRole>  m_signals;
        std::vector<SCommandRole> m_commands;
        std::vector<SInternalVariable> m_variables;
    };
}

#endif //NYM_PROJECT_CGENERICCOMPONENT_H
