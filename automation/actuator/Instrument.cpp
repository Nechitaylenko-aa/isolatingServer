#include "Instrument.h"
#include "NComponent.h"
#include "CCap.h"
#include "CParameter.h"

namespace NCore
{
    Instrument::Instrument(const SSignalRole &role)
        : m_role(role)
    {
    }

    const SSignalRole Instrument::role() const
    {
        return m_role;
    }

    void Instrument::set_tag(const Tstring &tag)
    {
        m_tag = tag;
    }

    Tstring Instrument::tag() const
    {
        return m_tag;
    }

    void Instrument::set_io_address(const Tstring &addr)
    {
        m_io_address = addr;
    }

    Tstring Instrument::io_address() const
    {
        return m_io_address;
    }
    //------------------------------------------------------------------------------------------------------------------
    //--- CPhysicalInstrument ------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------------------------

    CPhysicalInstrument::CPhysicalInstrument(CCap *anchor, const SSignalRole &role)
        : Instrument(role)
        , m_anchor(anchor)
    {
        Tstring in = (role.unit == E_MEASURE_UNITS::EMU_AMOUNT || role.unit ==E_MEASURE_UNITS::EMU_COUNT) ? "%DIX.X" : "%AIX.X";
        Tstring addr = in +" (" + signal_roles_str[role.role] + ")";
        set_io_address(addr);
        m_parameter = new CParameter(m_role.unit, m_role.subtype, 0, m_role.prefix);
        Tstring tag = anchor->get_owner()->get_description();
        tag += " id:" + std::to_string(anchor->get_owner()->get_id());
        m_tag = tag;
    }

    CPhysicalInstrument::~CPhysicalInstrument()
    {
        delete m_parameter;
    }

    CCap *CPhysicalInstrument::anchor() const
    {
        return m_anchor;
    }

    NComponent *CPhysicalInstrument::owner() const
    {
        return dynamic_cast<NComponent*>(m_anchor->get_owner());
    }

    CInfoBus *CPhysicalInstrument::info_bus() const
    {
        return owner()->info_bus();
    }

    float CPhysicalInstrument::value() const
    {

        // TODO(открытый пробел, не решено в этой сессии): корректно только для ролей,
        // читающих параметр COperatingBody по role().index (SR_PRESSURE_PV, SR_DP_FILTER
        // и подобные — то, что физически лежит в теле, текущем на CCap).
        // Роли вроде SR_PUMP_STATUS/SR_LEVEL_HIGH/SR_LEVEL_LOW читают не параметр РТ, а
        // состояние оборудования/дискретный датчик уровня — для них нужен отдельный путь
        // резолюции значения (через SEquipLight владельца или отдельный канал CInfoBus),
        // который в этой сессии не проектировался. Пока — единственный честный путь:
        // не выдавать это за готовое решение, а явно провалиться, чтобы не тестировать
        // на скрытом мусорном значении.
        COperatingBody *body = m_anchor->get_ob();
        if (!body)
        {
            return 0.0f;
        }

        uint16_t idx = role().index;

        CParameter *param = body->get_parameter(idx);
        if (!param)
        {
            return 0.0f;
        }

        return param->si_value();
    }

}
