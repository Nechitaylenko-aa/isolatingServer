//
// Created by artem on 28.05.26.
//

#ifndef NYM_PROJECT_CFILTERIONEXCHANGE_H
#define NYM_PROJECT_CFILTERIONEXCHANGE_H

#include "../../../include/NComponent.h"


namespace NCore
{
    class CFilterIonExchange : public NComponent
    {
    public:
        CFilterIonExchange(IGeneralTor * tor, CCell * owner);
        ~CFilterIonExchange() override;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

    protected:
        void set_equipment(equip::CEquipment *equip) override;
        void set_equipmentProxy(std::vector<SEquipLight> && items) override;

        std::vector<SSignalRole>           required_signals()      const override;
        std::vector<SCommandRole>          required_commands()     const override;
        std::vector<SAutomationSpec>       automation()             const override;
        std::vector<SDesignConstraintSpec> design_constraints()     const override;

    private:
        CParameter  * m_throughput_capacity; // пропускная способность
        CParameter  * m_average_filtration; // средняя способность фильтрования
        uint16_t    m_ser_comp_type,
                    m_ser_project_type;
        Tuint64 id_in{0}, id_out{0}, id_in1{0}, id_out1{0};

        void calculate_body();
    };

}

#endif //NYM_PROJECT_CFILTERIONEXCHANGE_H
