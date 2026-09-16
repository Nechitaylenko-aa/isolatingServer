//
// Аэратор — "относительно сложная, но полностью пассивная установка" (по твоему
// определению). Полностью пассивен в терминах Layer 2: required_commands() пуст
// (нами не управляется), automation() пуст. Проточный компонент, identity put_ob —
// как и у остальных "прозрачных" компонентов сейчас, реальный эффект на РТ (перенос
// кислорода и т.п.) НЕ смоделирован — это откладывается до отдельного обсуждения
// формулы/IShadow (см. ту же схему, что у CShadowLightFilter — расчёт с подобранным
// оборудованием), которого сейчас в задаче нет.
//
// design_constraints() оставлен пустым сознательно — "сложность" аэратора, судя по
// формулировке, скорее про паспортный расчёт производительности, чем про совместимость
// с соседями; без уточнения физики не выдумываю проверку.
//

#ifndef NYM_PROJECT_CAERATOR_H
#define NYM_PROJECT_CAERATOR_H

#include "NComponent.h"

namespace NCore
{
    class CAerator : public NComponent
    {
    public:
        CAerator(IGeneralTor *tor, CCell *owner);
        ~CAerator() override;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

    protected:
        void set_equipment(equip::CEquipment *equip) override {}
        void set_equipmentProxy(std::vector<SEquipLight> && items) override {}

        [[nodiscard]] std::vector<SSignalRole>           required_signals()      const override;
        [[nodiscard]] std::vector<SCommandRole>          required_commands()     const override;
        [[nodiscard]] std::vector<SAutomationSpec>       automation()             const override;
        [[nodiscard]] std::vector<SDesignConstraintSpec> design_constraints()     const override;
    };
}

#endif //NYM_PROJECT_CAERATOR_H
