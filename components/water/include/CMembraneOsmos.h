//
// Created by artem on 28.05.26.
//

#ifndef NYM_PROJECT_CMEMBRANEOSMOS_H
#define NYM_PROJECT_CMEMBRANEOSMOS_H

#include "../../../include/NComponent.h"

namespace NCore
{
    class CMembraneOsmos : public NComponent
    {
    public:
        explicit CMembraneOsmos(IGeneralTor * tor, CCell *owner);
        ~CMembraneOsmos() override;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

    protected:
        void set_equipment(equip::CEquipment *equip) override{}
        void set_equipmentProxy(std::vector<SEquipLight> && items) override{}

        std::vector<SSignalRole>           required_signals()      const override;
        std::vector<SCommandRole>          required_commands()     const override;
        std::vector<SAutomationSpec>       automation()             const override;
        std::vector<SDesignConstraintSpec> design_constraints()     const override;

    private:
        CParameter  * m_volume; // производительность - объем на время
        CParameter  * m_time;   //

    };
}


#endif //NYM_PROJECT_CMEMBRANEOSMOS_H
