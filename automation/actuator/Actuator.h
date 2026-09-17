//
// Layer-4: инстанс физического исполнительного устройства.
// Симметрично Instrument.h — тот же принцип: только AUTO_DERIVED, не сериализуется.
//

#ifndef NYM_PROJECT_ACTUATOR_H
#define NYM_PROJECT_ACTUATOR_H

//

#include "../CAutomationAtoms.h"

namespace NCore
{
    class CInfoBus;
    class NComponent;
    class CCap;

    class Actuator
    {
    public:
        Actuator() = delete;
        Actuator(const Actuator &) = delete;
        Actuator(Actuator &&) = delete;

        /**
         * @param anchor физический наконечник, через который Actuator достаёт компонент
         *               и информационную шину. Резолвится детерминированно на слое 2.
         * @param role   копия декларации из NComponent::required_commands().
         */
        Actuator(CCap *anchor, const SCommandRole &role);

        [[nodiscard]] CCap*       anchor() const;
        [[nodiscard]] NComponent* owner() const;
        //[[nodiscard]] CInfoBus*   info_bus() const;

        [[nodiscard]] const SCommandRole& role() const;

        void             set_tag(const Tstring &tag);
        [[nodiscard]] Tstring tag() const;

        //!< заглушка до появления слоя адресации по модулям ПЛК
        void             set_io_address(const Tstring &addr);
        [[nodiscard]] Tstring io_address() const;

        void  set_live_state(bool activated) { m_live_state = activated; }
        [[nodiscard]] bool live_state() const { return m_live_state; }

    private:
        CCap         * m_anchor;
        SCommandRole   m_role;
        Tstring        m_tag;
        Tstring        m_io_address{"%AQX.X"};
        bool m_live_state{false};
    };
}

#endif //NYM_PROJECT_ACTUATOR_H
