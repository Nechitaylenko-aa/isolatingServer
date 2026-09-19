//
// CSimAccountNode — минимальная тень узла учёта. Температура/давление НЕ её
// забота (CPhysicalInstrument читает их сам, через тело на анкер-кепке).
// Единственная работа: на каждый tick() спросить граф теней "кто сейчас
// фактически питает эту точку" и протолкнуть его паспортный расход как
// SR_FLOW_RATE — state-derived роль, которую CInternalInstrument читает
// через NComponent::read_state_signal().
//
// НЕ ICodegenShadow: у узла учёта нет развилки/арбитража, который нужно
// было бы резолвить в build_internal_topology() — обычный build_internal_atoms()
// с дефолтом на NComponent полностью достаточен, отдельный codegen-класс не нужен.
//

#ifndef NYM_PROJECT_CSIMACCOUNTNODE_H
#define NYM_PROJECT_CSIMACCOUNTNODE_H

#include "ISimulationShadow.h"

namespace NCore
{
    class CSimGraphManager;
    class CSimPumpStation;

    class CSimAccountNode : public ISimulationShadow
    {
    public:
        /**
         * @param component владелец (CAccountNode)
         * @param graph_manager не владеющий, тот же граф теней, что и у раннера —
         *        передаётся при создании тени в CSimShadowsManager, симметрично
         *        тому, как раннер сам получает CSimGraphManager
         */
        CSimAccountNode(NComponent *component);

        void  reset_runtime_state() override;
        void  tick(float dt) override;
        void  apply_command(const SCommandRole &role, bool activate) override; // no-op — узел учёта команд не принимает
        [[nodiscard]] float read(const SSignalRole &role, uint16_t index) const override;

    private:

        float m_last_rate{0.f};           // для read()/отладки — источник правды всё равно NComponent::m_state_signals
    };
}

#endif //NYM_PROJECT_CSIMACCOUNTNODE_H
