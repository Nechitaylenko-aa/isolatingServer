//
// Тень обратного осмоса — НЕ подбирает оборудование из БД (см. чат). getEquipRequest()
// всегда возвращает {} — компонент это уже трактует как "нечего слать" и не лезет в шину.
// Вся содержательная работа: (1) чистка воды — считается безусловно, без гейта на
// equip_id; (2) generateReport() формирует не формулы, а опросный лист (Specification) —
// заявку на приобретение для вендора.
//

#ifndef NYM_PROJECT_CSHADOWMEMBRANEOSMOS_H
#define NYM_PROJECT_CSHADOWMEMBRANEOSMOS_H

#include "../../IShadow.h"
#include <vector>
#include <tuple>
#include <utility>

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

class CShadowMembraneOsmos : public IShadow
{
public:
    CShadowMembraneOsmos() = delete;
    explicit CShadowMembraneOsmos(NCore::NComponent * component);
    ~CShadowMembraneOsmos() override = default;

    /** @brief Всегда {} — не подбор из БД, см. заголовок файла. */
    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    /** @brief Не вызывается по прямому назначению (equip_id никогда не станет ненулевым) —
     *  оставлен как пустая реализация чисто интерфейсно, не используется. */
    std::vector<float>  getCalculationsWithEquip(NCore::SComponentProxy & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                               const Tstring &section_number, uint32_t & formula_start) override;

    /** @brief Реальная чистка воды — вызывается компонентом напрямую из calculateInBody(),
     *  без гейта на оборудование. Возвращает [Q_пермеат, Q_концентрат], значения по body
     *  пишет сама (умножает каждую концентрацию на (1-rejection)) — компонент только
     *  раздаёт получившийся Q на два своих выхода. TODO(нет данных): recovery/rejection —
     *  документ прямо говорит, что точный подбор требует спец. софта (DuPont WAVE и т.п.),
     *  здесь только оценочные дефолты для солоноватой воды. */
    std::pair<float, float> purify(NCore::COperatingBody * body, IGeneralTor * tor);

private:
    struct SLastCalculation
    {
        bool  has_data{false};
        float Q_in{0.f};
        float Q_permeate{0.f};
        float Q_concentrate{0.f};
        float recovery{0.f};
        float rejection{0.f};
        float pressure_pa{0.f};
        std::vector<std::tuple<Tstring, float, Tstring>> input_composition;  // до purify()
    } m_last_calculation;
};

#endif //NYM_PROJECT_CSHADOWMEMBRANEOSMOS_H
