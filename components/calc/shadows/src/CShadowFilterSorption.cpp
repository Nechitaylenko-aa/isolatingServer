#include "../include/CShadowFilterSorption.h"
#include "NComponent.h"
#include "COperatingBody.h"
#include "Logger.h"

CShadowFilterSorption::CShadowFilterSorption(NCore::NComponent *component) : IShadow(component)
    , m_component(component)
{
}

NCore::SEquipmentRequest CShadowFilterSorption::getEquipRequest(NCore::COperatingBody *body, IGeneralTor *tor)
{
    // перенести реальную логику подбора оборудования из CShadowLightFilter,
    // как только будет доступен его .cpp — сейчас placeholder: пустой запрос с
    // владельцем-компонентом, params намеренно пуст (сигнализирует "нечего запрашивать"
    // в CFilterSorption::calculateInBody(), пока формула не перенесена).
    NCore::SEquipmentRequest request;
    request.sender = m_component;
    request.id_equip = 0;
    return request;
}

std::vector<float> CShadowFilterSorption::getCalculationsWithEquip(NCore::SEquipLight &equipProxy,
                                                                    NCore::COperatingBody *body, IGeneralTor *tor)
{
    // сюда переносится формула эффекта сорбционной загрузки на параметр "Мутность"
    // (или его наследника после перехода на enum) — placeholder, реальной физики нет.
    return {};
}

void CShadowFilterSorption::generateReport(NCore::COperatingBody *body, IGeneralTor *tor, EReportAction action)
{

    // automation_layer_context.md, не проработан вообще.
}
