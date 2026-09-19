//
// Created by artem on 20.08.26.
// Насосная станция — новый компонент, скелета не было. Написан по образцу CLightFilter.h.
//
// Единственный компонент из четырёх сегодняшних, у которого required_commands() НЕ пуст
// и automation() реально использует EA_ROTATION (единственный до сих пор нигде не
// использованный член EControlAlgorithm) — это тот "актуатор вверх по потоку", которого
// находит IInstallationTemplate::wire_boundary_patterns для SR_LEVEL_HIGH ёмкости.
//
//

#ifndef NYM_PROJECT_CPUMPSTATION_H
#define NYM_PROJECT_CPUMPSTATION_H

#include "../../../include/NComponent.h"
namespace NCore
{





    class CPumpStation : public NComponent
    {
    public:
        CPumpStation(IGeneralTor *tor, CCell *owner, uint16_t pump_count = 2);
        ~CPumpStation() override;

        COperatingBody* put_ob(COperatingBody *body, CCap* sender) override;

        bool     set_parameters(CContainer &container) override;
        void     get_parameters(CContainer & container) override;
        [[nodiscard]] Tstring  get_description() const override;

        void rebuild_internal_topology() override;
        [[nodiscard]] uint8_t     pump_count() const { return m_pump_count; }

        [[nodiscard]] std::vector<SCommandRole>          required_commands()     const override;
        [[nodiscard]] uint8_t     rotation_algorithm() const {return 0;};

    protected:
        void set_equipment(equip::CEquipment *equip) override {}
        void set_equipmentProxy(std::vector<SEquipLight> && items) override;
        /*[[nodiscard]] std::vector<SAutomationAtom> build_internal_atoms(
                const std::vector<SBoundaryInterlockCandidate> &external_candidates) const override;*/

        [[nodiscard]] std::vector<SVariableBehaviorSpec> variable_behaviors() const override;
        //[[nodiscard]] std::vector<SAggregateSignalSpec> aggregate_signals() const override;

        [[nodiscard]] std::vector<SSignalRole>           required_signals()      const override;

        [[nodiscard]] std::vector<SAutomationSpec>       automation()             const override;
        [[nodiscard]] std::vector<SDesignConstraintSpec> design_constraints()     const override;

    private:
        uint16_t     m_pump_count;
        CParameter * m_nominal_pressure; // паспортное давление на выходе станции
        std::vector<SEquipLight> m_equipmentChoice;

        void calculateInBody();
    };
}

#endif //NYM_PROJECT_CPUMPSTATION_H
