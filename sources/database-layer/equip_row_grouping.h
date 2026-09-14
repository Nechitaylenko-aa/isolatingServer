// db/equip_row_grouping.h
//
// Механическая группировка плоского результата CDatabaseModel::executeEquipQuery
// (см. database.md) в удобную для хендлеров подбора (Группа 4) структуру.
// Это НЕ алгоритм подбора - относится к слою БД, см.
// tz_group3_database_layer.md §4.
//
// Упорядоченность входных строк (natureType -> componentEnum -> idEquip)
// гарантируется самой закрытой реализацией CDatabaseModel (см. §4 ТЗ,
// решение по вопросу 1) - эта функция полагается на неё для группировки
// "тем же idEquip -> один SEquipGrouped", но не требует строгой
// отсортированности как инварианта: группировка выполняется через
// unordered_map по SEquipTypeKey и std::map<idEquip,...> внутри, поэтому
// корректна и при произвольном порядке строк (защита от неверного
// допущения об упорядоченности не стоит ничего по производительности при
// таких объёмах данных).
//
// Вызывается явно потребителем (по факту - Группа 4, на входе в
// IEquipMatchHandler::match()), CDBThread::executeEquipQuery эту функцию
// НЕ вызывает автоматически - см. §4 ТЗ.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/CDatabaseModel.h"

// Одна единица оборудования, собранная из N плоских SEquipRow с одинаковым
// idEquip (каждая строка SEquipRow - одно поле шаблона этого оборудования).
struct SEquipGrouped
{
    uint16_t    idEquip{0};
    uint16_t    idManufacturer{0};
    std::string equipName;

    struct SField
    {
        uint16_t    idTemplate{0};
        uint16_t    idTemplateField{0};
        std::string templFieldNameI;
        uint16_t    templFieldMeasureSetI{0};
        uint16_t    equipFieldMeasureI{0};
        uint8_t     equipFieldPrefixI{0};
        float       equipFieldValueF{0.f};
    };
    std::vector<SField> fields;
};

// Вход - плоский результат CDatabaseModel::executeEquipQuery.
// Выход - индекс: SEquipTypeKey (natureType+componentType) -> список
// SEquipGrouped (по idEquip). Строки с одинаковым
// (natureType, componentEnum, idEquip) схлопываются в один SEquipGrouped,
// каждая исходная строка добавляет один SField.
//
// equipName/idManufacturer берутся из первой встреченной строки данного
// idEquip. По построению они одинаковы во всех строках одного оборудования;
// если это не так (неконсистентность данных БД) - в debug-сборке
// срабатывает assert, в release расхождение молча игнорируется (используется
// значение из первой строки), но пишется CLogger::warn с идентификатором
// оборудования, чтобы проблему было видно в проде без падения сервиса.
std::unordered_map<SEquipTypeKey, std::vector<SEquipGrouped>, SEquipTypeKeyHash>
groupEquipRowsByType(const std::vector<SEquipRow>& rows);
