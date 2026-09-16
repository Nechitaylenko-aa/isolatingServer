//
// Станция воздуходувок — структурная копия CPumpStation. Алгоритм включения
// РАЗНЫЙ (воздуходувки чаще работают параллельно, включаются по мере роста
// нагрузки, а не "один рабочий + ротация по часам") — сама структура классов
// (компонент/codegen-тень/sim-тень) одинакова по прямому указанию автора,
// содержимое ротации — временная заглушка (копия pump-логики), правится
// отдельным заходом.
//

#ifndef NYM_PROJECT_CAIRBLOWER_H
#define NYM_PROJECT_CAIRBLOWER_H

#include "NComponent.h"

namespace NCore
{
    class CAirBlower : public NComponent
    {
    public:
        CAirBlower(IGeneralTor *tor, CCell *owner, uint16_t blower_count = 1);
        ~CAirBlower() override;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

        void rebuild_internal_topology() override;
        uint16_t   blower_count() const { return m_blower_count; }
        [[nodiscard]] std::vector<SCommandRole>          required_commands()     const override;

    protected:
        void set_equipment(equip::CEquipment *equip) override {}
        void set_equipmentProxy(std::vector<SEquipLight> && items) override {}


        [[nodiscard]] std::vector<SVariableBehaviorSpec> variable_behaviors() const override;
        [[nodiscard]] std::vector<SSignalRole>           required_signals()      const override;
        [[nodiscard]] std::vector<SAutomationSpec>       automation()             const override;
        [[nodiscard]] std::vector<SDesignConstraintSpec> design_constraints()     const override;

    private:
        uint16_t     m_blower_count{1};
        CParameter * m_nominal_pressure; // паспортное давление станции воздуходувок
    };
}

#endif //NYM_PROJECT_CAIRBLOWER_H
