//
// Симуляционная тень для CLightFilter.
// По аналогии с CSimFilterSorption — хранит ТОЛЬКО физическое состояние клапанов
// обратной промывки (открыт/закрыт). Никакой логики выбора момента старта/стопа —
// цикл промывки завершается по внешнему событию, приходящему как обычная command
// через wire_boundary_patterns/атомы, а не по внутреннему таймеру.
//

#ifndef NYM_PROJECT_CSIMLIGHTFILTER_H
#define NYM_PROJECT_CSIMLIGHTFILTER_H

#include "ISimulationShadow.h"
#include <array>

namespace NCore
{
    class CLightFilter;

    class CSimLightFilter : public ISimulationShadow
    {
    public:
        explicit CSimLightFilter(NComponent *component);
        ~CSimLightFilter() override = default;

        void  reset_runtime_state() override;
        void  tick(float dt) override;  // намеренно no-op — см. комментарий выше файла
        void  apply_command(const SCommandRole &role, bool activate) override;
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;

    private:
        [[nodiscard]] CLightFilter *filter() const;

        // index-aligned с required_commands() компонента: [0] и [1] — два клапана
        // обратной промывки (CR_FLOW_DIRECTION_VALVE, index 0/1)
        std::array<uint8_t, 2> m_valve_status{0, 0}; // 0 = закрыт, 1 = открыт
    };
}

#endif //NYM_PROJECT_CSIMLIGHTFILTER_H
