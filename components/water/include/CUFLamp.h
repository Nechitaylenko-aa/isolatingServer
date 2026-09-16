//
// UF-лампа (обеззараживание). Простой проточный компонент: РТ проходит насквозь без
// изменения измеримых параметров в этой модели (identity-эффект, как у CGenericComponent).
// Управление — булево вкл/выкл.
//
// TODO (требует правки CAutomationAtoms.h перед компиляцией):
//   - добавить в ECommandRole:  CR_LAMP_ON_OFF
//   - добавить в ESignalRole:   SR_LAMP_STATUS
//   - добавить SR_LAMP_STATUS в kStateDerivedSignalRoles (статус оборудования,
//     не параметр протекающего тела — нужна ISimulationShadow)
//   - не забыть parallel-массивы command_roles_str/signal_roles_str
//
// Открытый вопрос (не решается здесь, оставлен как есть): интерлок "не включать без
// потока" (сухой прогон) — design_constraints() ниже пуст, добавить по факту решения.
//

#ifndef NYM_PROJECT_CUFLAMP_H
#define NYM_PROJECT_CUFLAMP_H

#include "NComponent.h"

namespace NCore
{
    class CUFLamp : public NComponent
    {
    public:
        CUFLamp(IGeneralTor *tor, CCell *owner);
        ~CUFLamp() override;

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

#endif //NYM_PROJECT_CUFLAMP_H
