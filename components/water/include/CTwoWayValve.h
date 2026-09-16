//
// Created by artem on 23.08.26.
//

#ifndef NYM_PROJECT_CTWOWAYVALVE_H
#define NYM_PROJECT_CTWOWAYVALVE_H

#include "../../../include/NComponent.h"

namespace NCore
{
    /** @brief Двухходовой клапан. Топологически — обычный 1-вход/1-выход, как фильтр.
     *  Нормальное положение — ОТКРЫТ (проток идёт транзитом). Закрытое положение
     *  используется резолвером backwash-паттерна как переключение направления/байпас —
     *  сам клапан не знает, что именно он "заворачивает" или "байпасит", это знание
     *  живёт в SBackwashRecipe/overrides на уровне IWaterTreatmentTemplate, не здесь. */

    class CTwoWayValve : public NComponent
    {
    public:
        CTwoWayValve(IGeneralTor *tor, CCell *owner);
        ~CTwoWayValve() override;

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
        bool m_is_open{true};
        Tuint64  id_out1{0};
    };

} // NCore

#endif //NYM_PROJECT_CTWOWAYVALVE_H
