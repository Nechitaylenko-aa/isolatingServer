// equip/equip_match_handler.h
#pragma once

#include <vector>

#include "../protocol/parsed_types.h"        // SComponentReq (Группа 2)
#include "../protocol/packet_serializer.h"   // SEquipRespItem (Группа 2)
#include "../database-layer/equip_row_grouping.h"

class IEquipMatchHandler
{
public:
    virtual ~IEquipMatchHandler() = default;

    // equipPool - оборудование ОДНОГО SEquipTypeKey (natureType+componentType),
    //             уже сгруппированное по idEquip (groupEquipRowsByType, Группа 3).
    // request   - параметры конкретного компонента из запроса клиента.
    // Возвращает отобранные единицы оборудования, готовые к сериализации.
    // Не должен пробрасывать исключения как штатный способ сообщить "ничего не
    // найдено" - для этого возвращается пустой vector. Диспетчер оборачивает
    // вызов в try/catch как защитную сетку, а не как механизм управления потоком.
    virtual std::vector<SEquipRespItem> match(const std::vector<SEquipGrouped>& equipPool,
                                               const SComponentReq&              request) = 0;
};
