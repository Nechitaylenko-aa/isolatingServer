//
// Отсекающий клапан (вкл/выкл) — Layer 1/2. Не путать с уже существующим
// CSimValveTwoDirectional (двунаправленный клапан) — этот компонент проще:
// один вход/выход, один статус "открыт/закрыт", без направления.
//

#ifndef NYM_PROJECT_CVALVECUT_H
#define NYM_PROJECT_CVALVECUT_H

#include "NComponent.h"

namespace NCore
{
    class CValveCut : public NComponent
    {
    public:
        CValveCut(IGeneralTor *tor, CCell *owner);
        ~CValveCut() override = default;

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

#endif //NYM_PROJECT_CVALVECUT_H
