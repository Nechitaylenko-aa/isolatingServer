//
// Created by artem on 30.10.25.
//

#ifndef NYM_PROJECT_CLIGHTFILTER_H
#define NYM_PROJECT_CLIGHTFILTER_H


#include "../../../include/NComponent.h"

static Tstring mutnost = "Мутность";

namespace NCore
{
    class CLightFilter : public NComponent
    {
    public:
        CLightFilter(IGeneralTor *tor, CCell *owner);
        ~CLightFilter() override;

        /** @brief единственны метод для всех двух-пиновых компонентов для работы над телом*/
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

        float m_F_required, m_Q, m_pressure;
        Tuint64 id_in1{0}, id_out1{0};

        void calculateInBody();
    };
}


#endif //NYM_PROJECT_CLIGHTFILTER_H
