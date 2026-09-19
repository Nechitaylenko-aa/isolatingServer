//
// Created by artem on 17.06.26.
//

#ifndef NYM_PROJECT_ISHADOW_H
#define NYM_PROJECT_ISHADOW_H

#include "CInfoBus.h"
#include "IShadowManager.h"
//#include "../include/COperatingBody.h"

static const std::string Mutnost = "Мутность";
/** @brief Базовый класс для "теней" компонентов, которые берут на себя 2 роли, освобождая NComponent от тяжелых функций:
 * - расчет параметров для подбора оборудования из БД;
 * - генерация документации по расчету параметров для выбора оборудования; */
namespace NCore {
    class COperatingBody;
}
class IGeneralTor;

class IShadow
{
public:
    IShadow() = delete;
    IShadow(const IShadow &) = delete;
    IShadow(IShadow &&) = delete;
    virtual ~IShadow() = default;

    virtual  NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) = 0; // рассчитать параметры и дать структуру для подбора (отправки на сервер) для конкретного компонента
    virtual  std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equip_proxy, NCore::COperatingBody * body, IGeneralTor *tor) = 0;
    virtual  std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                                         const Tstring &section_number, uint32_t & formula_start) = 0; // сгенерировать отчет по компоненту для документации
    [[nodiscard]] const NCore::NComponent * component() const { return m_component; }

protected:
    NCore::NComponent * m_component;

protected:
    explicit IShadow(NCore::NComponent * component) : m_component(component){};
};

#endif //NYM_PROJECT_ISHADOW_H
