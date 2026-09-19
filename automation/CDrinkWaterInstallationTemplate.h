//
// Шаг 2 (минимальный сквозной сценарий): не полный резолвер под все правила ToR,
// а один захардкоженный случай — собрать один SAtomSet с одним атомом для первого
// найденного компонента с TT_SCHEDULE (CLightFilter). Цель — увидеть живую цепочку
// хук -> bind() -> объект в CAutomationModel, не покрыть все установки сразу.
//
// см. automation_layer_runner_architecture.md, §"С чего начнём" / шаг 2
//

#ifndef NYM_PROJECT_CDRINKWATERINSTALLATIONTEMPLATE_H
#define NYM_PROJECT_CDRINKWATERINSTALLATIONTEMPLATE_H

#include "IWaterTreatmentTemplate.h"

namespace NCore
{
    class CDrinkWaterInstallationTemplate : public IWaterTreatmentTemplate
    {
    public:
        bool bind(CSubProject *proj, const SCollectedAutomationData &data) override;
        float resolve_tor_threshold(ESetpointSource source, const SCondition &cond) const override;

    protected:

    };
}

#endif //NYM_PROJECT_CDRINKWATERINSTALLATIONTEMPLATE_H
