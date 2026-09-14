// db/equip_row_grouping.cpp

#include "equip_row_grouping.h"

#include <cassert>
#include <unordered_map>

#include "../logger-common/logger.h"


std::unordered_map<SEquipTypeKey, std::vector<SEquipGrouped>, SEquipTypeKeyHash>
groupEquipRowsByType(const std::vector<SEquipRow>& rows)
{
    std::unordered_map<SEquipTypeKey, std::vector<SEquipGrouped>, SEquipTypeKeyHash> result;

    // Для каждого SEquipTypeKey - индекс idEquip -> позиция в result[key],
    // чтобы не делать линейный поиск по уже накопленным SEquipGrouped.
    std::unordered_map<SEquipTypeKey, std::unordered_map<uint16_t, size_t>, SEquipTypeKeyHash> indexByKey;

    for (const auto& row : rows)
    {
        const SEquipTypeKey key{static_cast<NCore::EComponentTypes>(row.natureType), row.componentEnum};

        auto& groupedVec  = result[key];
        auto& idEquipIndex = indexByKey[key];

        auto it = idEquipIndex.find(row.idEquip);
        if (it == idEquipIndex.end())
        {
            SEquipGrouped grouped;
            grouped.idEquip         = row.idEquip;
            grouped.idManufacturer  = row.id_manufacturer;
            grouped.equipName       = row.equipName;

            groupedVec.push_back(std::move(grouped));
            it = idEquipIndex.emplace(row.idEquip, groupedVec.size() - 1).first;
        }
        else
        {
            // Инвариант: equipName/id_manufacturer одинаковы во всех строках
            // одного idEquip по построению (database.md). Расхождение -
            // сигнал неконсистентности данных в БД: не падаем, используем
            // значение первой встреченной строки, но фиксируем в лог.
            SEquipGrouped& existing = groupedVec[it->second];
            assert(existing.equipName == row.equipName && "SEquipRow: equipName расходится в пределах одного idEquip");
            assert(existing.idManufacturer == row.id_manufacturer && "SEquipRow: id_manufacturer расходится в пределах одного idEquip");

            if (existing.equipName != row.equipName || existing.idManufacturer != row.id_manufacturer)
            {
                CLogger::instance().warn(
                    "equip_row_grouping",
                    "inconsistent SEquipRow for idEquip={}: equipName/id_manufacturer differ between rows, first-seen value kept",
                    row.idEquip);
            }
        }

        SEquipGrouped::SField field;
        field.idTemplate            = row.idTemplate;
        field.idTemplateField        = row.idTemplateField;
        field.templFieldNameI        = row.templFieldNameI;
        field.templFieldMeasureSetI  = row.templFieldMeasureSetI;
        field.equipFieldMeasureI     = row.equipFieldMeasureI;
        field.equipFieldPrefixI      = row.equipFieldPrefixI;
        field.equipFieldValueF       = row.equipFieldValueF;

        groupedVec[it->second].fields.push_back(std::move(field));
    }

    return result;
}
