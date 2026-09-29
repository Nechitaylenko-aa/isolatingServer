//
// Created by artem on 23.08.26.
//

#ifndef NYM_PROJECT_CTHREEWAYVALVE_H
#define NYM_PROJECT_CTHREEWAYVALVE_H

#include "../../../include/NComponent.h"

namespace NCore
{
    enum class EValvePosition : uint8_t { A_TO_B, A_TO_C };

    /** @brief Трёхходовой клапан-переключатель (diverting), L-port. 1 вход, 2 выхода —
     *  топология уже была готова, доделана только маршрутизация по позиции.
     *  Позицией управляет automation-слой (концевики/привод) — тень (calc) её не решает,
     *  только подбирает железо (DN/Kv/PN) один раз, на этапе проектирования, см. чат. */
    class CThreeWayValve : public NComponent
    {
    public:
        CThreeWayValve(IGeneralTor *tor, CCell *owner);
        ~CThreeWayValve() override;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

        /** @brief Вызывается automation-слоем (не тенью) при смене положения. */
        void set_position(EValvePosition pos) { m_position = pos; }

    protected:
        void set_equipment(equip::CEquipment *equip) override;
        void set_equipmentProxy(std::vector<SComponentProxy> && items) override;

        std::vector<SSignalRole>           required_signals()      const override;
        std::vector<SCommandRole>          required_commands()     const override;
        std::vector<SAutomationSpec>       automation()             const override;
        std::vector<SDesignConstraintSpec> design_constraints()     const override;

    private:
        bool m_is_open{true};
        EValvePosition m_position{EValvePosition::A_TO_B};
        Tuint64  id_out1{0};
        std::vector<SComponentProxy> m_equipmentChoice;

        void calculateInBody();
    };

} // NCore

#endif //NYM_PROJECT_CTHREEWAYVALVE_H
