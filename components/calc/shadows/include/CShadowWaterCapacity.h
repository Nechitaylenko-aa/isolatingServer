//
// Создано по образцу CShadowLightFiletr.h
//

#ifndef NYM_PROJECT_CSHADOWWATERCAPACITY_H
#define NYM_PROJECT_CSHADOWWATERCAPACITY_H

#include "../../IShadow.h"
#include <optional>

namespace NCore
{
    class NComponent;
    class COperatingBody;
}

/** @brief Тень ёмкости — в отличие от фильтров, не обрабатывает протекающий параметр
 *  (put_ob у CWaterCapacity — тождественное преобразование, см. комментарий в
 *  CWaterCapacity.cpp), а только подбирает габариты (ширина/глубина/высота) под нужный
 *  объём. Динамика уровня во времени — отдельный будущий механизм (CSimulationShadow),
 *  сюда не входит. */
class CShadowWaterCapacity : public IShadow
{
public:
    CShadowWaterCapacity() = delete;
    explicit CShadowWaterCapacity(NCore::NComponent * component);
    ~CShadowWaterCapacity() override = default;

    NCore::SEquipmentRequest getEquipRequest(NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<float>  getCalculationsWithEquip(NCore::SEquipLight & equipProxy, NCore::COperatingBody * body, IGeneralTor *tor) override;
    std::vector<SReportEntry>  generateReport(NCore::COperatingBody * body, IGeneralTor *tor, EReportAction action,
                                               const Tstring &section_number, uint32_t & formula_start) override;

    /** @brief Минимальный напор, который должен обеспечить насос выше по потоку, чтобы
     *  налить эту ёмкость доверху: rho * g * height выбранной ёмкости. Пусто, пока
     *  оборудование ещё не подобрано (has_data == false) — вызывающий (насос) в этом
     *  случае просто ничего не запрашивает в этом цикле, см. чат про многостадийность. */
    [[nodiscard]] std::optional<float> min_required_pressure_pa() const;

private:
    struct SLastCalculation
    {
        bool    has_data{false};
        float   width{0.f};
        float   deepness{0.f};
        float   height{0.f};
        float   volume{0.f};
    } m_last_calculation;

    /** @brief Обходит граф вверх по потоку от входа ёмкости, находит фильтры (свет/ионный обмен),
     *  суммирует их потребность в воде на промывку/отмывку через конкретные геттеры их теней
     *  (не через IShadow — общего интерфейса для этого сознательно нет, см. чат). nullopt —
     *  выше по потоку есть фильтр, у которого оборудование ещё не подобрано (геттер вернул
     *  nullopt) — ёмкость в этом случае должна подождать следующего цикла, а не считать её
     *  как 0. Пустой список соседей (фильтров нет вообще) — не ошибка, возвращает 0.f. */
    [[nodiscard]] std::optional<float> sum_upstream_backwash_demand() const;
};

#endif //NYM_PROJECT_CSHADOWWATERCAPACITY_H
