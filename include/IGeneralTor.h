//
// Created by artem on 25.05.24.
//

#ifndef NYM_PROJECT_IGENERALTOR_H
#define NYM_PROJECT_IGENERALTOR_H


//#include "saver_export.h"
#include "core-types.h"
#include "CLimits.h"
#include "CParameter.h"
#include "serial_interfaces.h"


class CSubProject;
namespace NCore
{
    class COperatingBody;
}
/**
 * @brief Основное ТЗ для установки. Только установки бывают разные и, соответственно, ТЗ у них разные, так что
 * тут будет херова туча классов-наследников с данными, которые будут описывать эти самые ТЗ, т.к. у этих ТЗ
 * будут разные структуры и методы, общий интерфейс будет незатейливым
 * @attention не забудьте в наследниках создать рабочее и эталонное (где приемлимо) рабочие тела. Хранятся и
 * уничтожаются они в этом классе!
 */
class  IGeneralTor : public ISerializable
{
public:
    IGeneralTor();
    ~IGeneralTor() override;

    static IGeneralTor*     create_tor(CSubProject *parent, const NCore::E_PROJECT_TYPE &project_type);

    virtual  void  Delete() = 0;
    virtual  CSubProject*  parent() = 0;
    [[nodiscard]] virtual  bool  is_ready() const = 0;  //!< все ли данные заполнены

    virtual void    set_name(const Tstring &name) = 0;
    [[nodiscard]] virtual Tstring name() const = 0;

    [[nodiscard]] virtual uint8_t base_inputs() const = 0;
    virtual void  set_base_inputs(uint8_t inputs) = 0;

    [[nodiscard]] virtual uint8_t  base_outputs() const = 0;
    virtual void set_base_outputs(uint8_t outputs) = 0;

    [[nodiscard]] virtual NCore::E_PROJECT_TYPE   type() const = 0;
    virtual std::vector<NCore::COperatingBody *> * operating_bodies() = 0;
    virtual NCore::COperatingBody*  general_working_body() = 0;
    virtual NCore::COperatingBody*  reference_body() = 0;

    virtual CLimits*    limit_at(const uint8_t & index) = 0;
    [[nodiscard]] virtual uint8_t     limits_count() const = 0;

    CParameter  * hourInputMax();

    /// serialization ----------------------------
    [[nodiscard]] EObjectType   get_type() const final {return OT_GeneralTor; }
    [[nodiscard]] NCore::TComponentType get_subtype() const final { return {}; }
    [[nodiscard]] Tstring  get_description() const override { return name(); }

protected:

    CParameter  * m_incomeHourMax;

    std::vector<NCore::COperatingBody*> * m_working_bodies{nullptr};      //!< рабочее-рабочее тело, оно ходит по системе
    std::vector<NCore::COperatingBody*> * m_reference_bodies{nullptr};    //!< эталонное тело (параметры очистки по СНиП и подобное),
                                                            //!< его может не быть
    std::vector<CParameter*>  m_parameters; //!< параметры создаются и задаются в конкретных ТЗ (наследниках), но владеет ими этот класс

    uint8_t  m_base_inputs{1};
    uint8_t  m_base_outputs{1};
};




#endif //NYM_PROJECT_IGENERALTOR_H
