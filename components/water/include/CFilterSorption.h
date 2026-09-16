#ifndef NYM_PROJECT_CFILTERSORPTION_H
#define NYM_PROJECT_CFILTERSORPTION_H

#include "../../../include/NComponent.h"

namespace NCore
{
    class CFilterSorption : public NComponent
    {
    public:
        explicit CFilterSorption(IGeneralTor * tor, CCell *owner);
        ~CFilterSorption() override;

        /** @brief единственный метод для всех двух-пинных (точнее четырёх-пинных, с учётом
         *  CT_ADDITIONAL) компонентов для работы над телом — по образцу CLightFilter */
        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

    protected:
        void set_equipment(equip::CEquipment *equip) override {}
        void set_equipmentProxy(std::vector<SEquipLight> && items) override;

        std::vector<SSignalRole>           required_signals()      const override;
        std::vector<SCommandRole>          required_commands()     const override;
        std::vector<SAutomationSpec>       automation()             const override;
        std::vector<SDesignConstraintSpec> design_constraints()     const override;

    private:
        CParameter  * m_throughput_capacity; // пропускная способность (объём загрузки)
        CParameter  * m_average_filtration;  // средняя способность фильтрования/сорбции

        Tuint64 id_in1{0}, id_out1{0};        // сериализация второй (CT_ADDITIONAL) пары кепок
        std::vector<SEquipLight> m_equipmentChoice;

        /** @brief расчёт эффекта на входящем теле — по образцу CLightFilter::calculateInBody(),
         *  через IShadowManager (запрос/применение оборудования) */
        void calculateInBody();
    };
}

#endif //NYM_PROJECT_CFILTERSORPTION_H
