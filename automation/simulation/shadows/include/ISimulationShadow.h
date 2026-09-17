//
// Created by artem on 28.08.26.
//

#ifndef NYM_PROJECT_ISIMULATIONSHADOW_H
#define NYM_PROJECT_ISIMULATIONSHADOW_H

#include "../../../CAutomationAtoms.h"



namespace NCore {

    class ISimulationShadow
    {
    public:
        virtual ~ISimulationShadow() = default;
        virtual void  reset_runtime_state() = 0;   // инициализация перед стартом раннера/перезапуском симуляции
        virtual void  tick(float dt) = 0;
        virtual void  apply_command(const SCommandRole &role, bool activate) = 0;
        [[nodiscard]] virtual float read(const SSignalRole &role, uint16_t index) const = 0;
        virtual void set_topology_changed_callback(std::function<void()>) {}
        [[nodiscard]] virtual std::vector<NCore::SSignalRole> readable_state_roles() const { return {}; }
        [[nodiscard]] NComponent* owner() const { return m_component; }
    protected:
        explicit ISimulationShadow(NComponent *component) : m_component(component) {}
        NComponent *m_component;
    };

}


#endif //NYM_PROJECT_ISIMULATIONSHADOW_H
