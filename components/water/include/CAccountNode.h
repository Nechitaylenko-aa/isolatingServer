//
// CAccountNode — "узел учёта". Врезается В РАЗРЕЗ трубопровода (РТ реально
// проходит через него), поэтому обычный NComponent со своими Cap и
// put_ob ≈ identity — не отдельная сущность типа Instrument (см.
// automation_layer_context.md, раздел "Instrument/Actuator без Cap vs узел учёта").
//
// Три величины разной природы:
//  - температура, давление — параметры COperatingBody, читаются штатно через
//    CPhysicalInstrument (НЕ state-derived, ничего специального не требуют);
//  - расход (мгновенный) — НЕ поле COperatingBody, state-derived роль,
//    значение поставляет CSimAccountNode через read_state_signal();
//  - расход с накоплением (тотализатор) — internal variable
//    (VB_ACCUMULATE_RATE, см. патч CAutomationAtoms.h), retain, растёт на
//    SR_FLOW_RATE * dt каждый тик, обслуживается общим механизмом раннера,
//    tень для него отдельно ничего не делает.
//

#ifndef NYM_PROJECT_CACCOUNTNODE_H
#define NYM_PROJECT_CACCOUNTNODE_H

#include "../../../include/NComponent.h"

namespace NCore
{
    class CAccountNode : public NComponent
    {
    public:
        CAccountNode(IGeneralTor *tor, CCell *owner);
        ~CAccountNode() override;

        /** @brief единственный метод для двух-пинового узла учёта — тело проходит
         *  насквозь без изменения (identity), сама величина не влияет на состав РТ. */
        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

        CParameter*   diameter() const;
        COperatingBody * ob() const;

    protected:
        void set_equipment(equip::CEquipment *equip) override;
        void set_equipmentProxy(std::vector<SEquipLight> && items) override;

        std::vector<SSignalRole>           required_signals()      const override;
        std::vector<SCommandRole>          required_commands()     const override; // пусто — ничем не управляет
        std::vector<SAutomationSpec>       automation()             const override; // пусто — нет своего алгоритма
        std::vector<SDesignConstraintSpec> design_constraints()     const override; // пусто, пока нет проверки

        /** @brief тотализатор расхода — retain-переменная, накапливающая
         *  SR_FLOW_RATE во времени. Единственное содержательное переопределение
         *  этого хука у данного компонента. */
        std::vector<SVariableBehaviorSpec> variable_behaviors() const override;

    private:
        Tuint64 id_in{0}, id_out{0};
        CParameter  * m_parameter;
    };
}

#endif //NYM_PROJECT_CACCOUNTNODE_H
