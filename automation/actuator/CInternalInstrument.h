//
// Created by artem on 24.08.26.
//

#ifndef NYM_PROJECT_CINTERNALINSTRUMENT_H
#define NYM_PROJECT_CINTERNALINSTRUMENT_H

// actuator/CInternalInstrument.h
//
// Сигнал без физической CCap — существует только как (owner, role, index). Для
// датчиков перегрева/завоздушенности КОНКРЕТНОГО насоса внутри станции: физически
// это реальные DI, но у насоса нет своего NComponent/Cap в графе — станция и есть
// его единственный "домен". Не CPhysicalInstrument (нечего читать с Cap) и не
// CServiceInstrument (не вычисляемое значение, а обычный физический вход) —
// поэтому третий, самый простой вид.

#include "Instrument.h"

namespace NCore
{
    class NComponent;

    class CInternalInstrument : public Instrument
    {
    public:
        CInternalInstrument(NComponent *owner, const SSignalRole &role);

        /*
        [[nodiscard]] NComponent*  owner() const { return m_owner; }
        [[nodiscard]] SSignalRole  role() const override { return m_role; }
        [[nodiscard]] Tstring      tag() const override;
        [[nodiscard]] Tstring      io_address() const override { return m_io_address; }
        void  set_io_address(const Tstring &addr) { m_io_address = addr; } // слой Б, когда появится каталог модулей
        */
        [[nodiscard]] const SSignalRole role() const override;
        void   set_value(float v) { m_value = v; }   // пишет Shadow напрямую (симуляция/дебаг), не через traversal
        [[nodiscard]] float value() const override; //return m_value; }
        NComponent  * owner() const override;
        //void set_signal(ESignalRole signal_role) override;

    private:
        NComponent  *m_owner;
        //SSignalRole  m_role; in the base class is
        //Tstring      m_io_address{"%IX?.?"};
        float        m_value{0};
    };
}

#endif //NYM_PROJECT_CINTERNALINSTRUMENT_H
