//
// Created by artem on 16.06.26.
//

#ifndef NYM_PROJECT_ISHADOWMANAGER_H
#define NYM_PROJECT_ISHADOWMANAGER_H

#include "CInfoBus.h"
//#include "CEquipment.h"


class IGeneralTor;

namespace NCore {
    class COperatingBody;
}

class IShadow;

enum EReportAction
{

};

/** @brief Вид строки отчёта — определяет, как рендерер её оформит (заголовок жирным, формула —
 *  отдельным блоком с номером и т.д.). Сама тень не знает про HTML/PDF, только про смысл строки. */
enum class EReportEntryKind : uint8_t
{
    SectionHeading,  // "2.2. Расчет фильтра осветлительного ФО23"
    Text,            // обычный поясняющий текст
    Formula,         // формула + подставленные значения + результат, нумеруется
};

/** @brief Одна строка отчёта. Для Formula — три текстовых поля вместо одной свободной строки
 *  специально: символьная формула, она же с подставленными значениями, и результат (см. самое
 *  начало обсуждения). Если бы это была одна строка, рендер (HTML/PDF) пришлось бы либо парсить
 *  обратно на эти части, либо каждая из 12 теней решала бы форматирование сама. */
struct SReportEntry
{
    EReportEntryKind kind{EReportEntryKind::Text};

    Tstring  text;              // для SectionHeading/Text — готовая строка
    Tstring  parameter_name;    // для Formula — "Мутность на выходе"
    Tstring  formula_symbolic;  // "C_out = C_in * (1 - eta)"
    Tstring  substituted;       // "C_out = 12.4 * (1 - 0.82)"
    float    result{0.f};
    Tstring  unit;              // "ЕМФ"
    uint32_t formula_number{0}; // "(6)" — только для kind==Formula, сквозная нумерация по документу
};

class IShadowManager
{
public:
    IShadowManager(const IShadowManager &) = delete;
    IShadowManager(IShadowManager &&) = delete;
    IShadowManager& operator=(const IShadowManager &) = delete;
    ~IShadowManager();

    static  NCore::SEquipmentRequest  getEquipRequest(NCore::NComponent * component, IGeneralTor * tor, NCore::COperatingBody *body);
    static  std::vector<float>        getBodyParams(NCore::NComponent *component, NCore::SEquipLight &equip_proxy, IGeneralTor * tor, NCore::COperatingBody *body);
    static  std::vector<SReportEntry>  generateReport(NCore::NComponent * component, IGeneralTor * tor, NCore::COperatingBody *body,
                                                        EReportAction action, const Tstring &section_number, uint32_t formula_start);

    /** @brief Публичный доступ к тени соседнего компонента — нужен, когда одна тень спрашивает
     *  другую напрямую (см. чат про насос/ёмкость/фильтр), в отличие от getEquipRequest/
     *  getBodyParams/generateReport, которые сами вызывают нужный метод и ничего не отдают наружу.
     *  Вызывающий сам знает подтип соседа (нашёл его предикатом) и кастует результат сам. */
    static  IShadow*  getComponentShadow(NCore::NComponent * component);

private:
    IShadowManager();
    static IShadowManager &instance();
    IShadow* getShadow(NCore::NComponent * component);
    std::vector<IShadow*>  m_shadows;

    IShadow* getWaterShadow(NCore::E_WATER_COMPONENTS subtype, NCore::NComponent* component);
    IShadow* getGasShadow(NCore::E_GAS_COMPONENTS subtype, NCore::NComponent* component);
    IShadow* getElectricShadow(NCore::E_ELECTRIC_COMPONENTS subtype, NCore::NComponent* component);
};


#endif //NYM_PROJECT_ISHADOWMANAGER_H
