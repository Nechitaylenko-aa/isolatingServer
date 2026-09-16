//
// Created by artem on 28.05.26.
//

#ifndef NYM_PROJECT_CWATERCAPACITY_H
#define NYM_PROJECT_CWATERCAPACITY_H

#include "../../../include/NComponent.h"

namespace NCore
{
    class CWaterCapacity : public NComponent
    {
    public:
        explicit CWaterCapacity(IGeneralTor * tor, CCell *owner);
        ~CWaterCapacity() override;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

        std::vector<SSignalRole>           required_signals()      const override;
        std::vector<SCommandRole>          required_commands()     const override;
        std::vector<SAutomationSpec>       automation()             const override;
        std::vector<SDesignConstraintSpec> design_constraints()     const override;
        bool is_flow_boundary() const override { return true;}
        //void set_signal(ESignalRole role) override;
        //std::vector<SAggregateSignalSpec>    aggregate_signals() const override;


    protected:
        void set_equipment(equip::CEquipment *equip) override {}
        void set_equipmentProxy(std::vector<SEquipLight> && items) override {}

    private:
        CParameter  * m_volume;
        CParameter  * m_width;
        CParameter  * m_deepness;
        CParameter  * m_height;
        ESignalRole   m_signal_role;

        Tuint64   m_idIn1{0}, m_idIn2;

    };
}


#endif //NYM_PROJECT_CWATERCAPACITY_H
