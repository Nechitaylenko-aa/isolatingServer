//
// Единственный сегодняшний пример служебной величины — SR_TIME_SINCE_SET_ACTIVE,
// закрывает TT_SCHEDULE как частный случай SCondition, без отдельного вида триггера.
// anchor отсутствует по смыслу — здесь нечего резолвить на слое 2, источник значения
// подставляется тем, кто создаёт инструмент (обычно CAutomationModel/IInstallationTemplate).
//
// см. automation_layer_runner_architecture.md, §4
//

#ifndef NYM_PROJECT_CSERVICEINSTRUMENT_H
#define NYM_PROJECT_CSERVICEINSTRUMENT_H

#include "Instrument.h"
#include <functional>

namespace NCore
{
    class CServiceInstrument : public Instrument
    {
    public:
        CServiceInstrument() = delete;
        CServiceInstrument(const CServiceInstrument &) = delete;
        CServiceInstrument(CServiceInstrument &&) = delete;

        CServiceInstrument(const SSignalRole &role, std::function<float()> source)
            : Instrument(role)
            , m_source(std::move(source))
        {
        }

        NComponent * owner() const override { return nullptr; }

        [[nodiscard]] float value() const override
        {
            return m_source ? m_source() : 0.0f;
        }

    private:
        std::function<float()> m_source;
    };
}

#endif //NYM_PROJECT_CSERVICEINSTRUMENT_H
