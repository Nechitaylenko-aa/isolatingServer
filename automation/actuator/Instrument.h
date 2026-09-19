//
// Layer-4: источник READ для атома. Полиморфный интерфейс — подавляющее большинство
// случаев физические (CPhysicalInstrument), но модель уже сейчас предвидит служебные
// величины раннера (CServiceInstrument.h) и, вероятно, ещё что-то в будущем (SCADA/оператор) —
// наследование от общего интерфейса, не декорация одного конкретного класса.
//
// см. automation_layer_runner_architecture.md, §4
//

#ifndef NYM_PROJECT_INSTRUMENT_H
#define NYM_PROJECT_INSTRUMENT_H

#include <core-types.h>
#include "../CAutomationAtoms.h"


class CParameter;

namespace NCore
{
    class CInfoBus;
    class NComponent;
    class CCap;

    class Instrument
    {
    public:
        Instrument(const Instrument &) = delete;
        Instrument(Instrument &&) = delete;
        virtual ~Instrument() = default;



        /** @brief единственное, что нужно атому для READ. Физический экземпляр читает
         *  через CInfoBus/CParameter, служебный — через свой лямбда-источник. */
        [[nodiscard]] virtual float value() const = 0;

        [[nodiscard]]virtual const SSignalRole role() const;
        //virtual void set_signal(ESignalRole signal_role) {}

        void             set_tag(const Tstring &tag);
        [[nodiscard]] Tstring tag() const;

        //!< заглушка до появления слоя адресации по модулям ПЛК; для служебных
        //!< инструментов остаётся пустой строкой — им нечего адресовать физически
        void             set_io_address(const Tstring &addr);
        [[nodiscard]] Tstring io_address() const;
        [[nodiscard]] virtual NComponent* owner() const = 0;

    protected:
        explicit Instrument(const SSignalRole &role);


        SSignalRole   m_role;
        Tstring       m_tag;
        Tstring       m_io_address;
    };

    /** @brief Физический прибор на реальном CCap. anchor обязателен, не nullptr —
     *  резолвится детерминированно на слое 2 (CAutomationCollector::resolve_anchor). */
    class CPhysicalInstrument : public Instrument
    {
    public:
        CPhysicalInstrument() = delete;
        CPhysicalInstrument(const CPhysicalInstrument &) = delete;
        CPhysicalInstrument(CPhysicalInstrument &&) = delete;

        CPhysicalInstrument(CCap *anchor, const SSignalRole &role);
        ~CPhysicalInstrument() override;

        [[nodiscard]] float value() const override;

        [[nodiscard]] CCap*       anchor() const;
        [[nodiscard]] NComponent* owner() const override;    //!< anchor()->get_owner(), приведённый к NComponent
        [[nodiscard]] CInfoBus*   info_bus() const; //!< owner()->info_bus()

    private:
        CCap * m_anchor;
        CParameter  * m_parameter;
    };
}

#endif //NYM_PROJECT_INSTRUMENT_H
