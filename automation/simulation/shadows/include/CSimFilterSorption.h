//
// Симуляционная тень для CFilterSorption.
// Хранит ТОЛЬКО физическое состояние клапанов обратной промывки (открыт/закрыт).
// Никакой внутренней логики выбора момента старта/стопа здесь нет и не должно быть —
// цикл промывки не имеет фиксированной длительности, он завершается по внешнему
// событию (например, уровень в приёмной ёмкости промывной воды), которое приходит
// как обычная command через wire_boundary_patterns/атомы — так же, как старт/стоп
// насоса приходит станции. Тень не знает "почему", только "что сейчас физически
// открыто/закрыто".
//

#ifndef NYM_PROJECT_CSIMFILTERSORPTION_H
#define NYM_PROJECT_CSIMFILTERSORPTION_H

#include "ISimulationShadow.h"
#include <array>

namespace NCore
{
    class CFilterSorption;

    class CSimFilterSorption : public ISimulationShadow
    {
    public:
        explicit CSimFilterSorption(NComponent *component);
        ~CSimFilterSorption() override = default;

        void  reset_runtime_state() override;
        void  tick(float dt) override;  // намеренно no-op — см. комментарий выше файла
        void  apply_command(const SCommandRole &role, bool activate) override;
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;

    private:
        [[nodiscard]] CFilterSorption *filter() const;

        // index-aligned с required_commands() компонента: [0] и [1] — два клапана
        // обратной промывки (CR_FLOW_DIRECTION_VALVE, index 0/1)
        std::array<uint8_t, 2> m_valve_status{0, 0}; // 0 = закрыт, 1 = открыт
    };
}

#endif //NYM_PROJECT_CSIMFILTERSORPTION_H
