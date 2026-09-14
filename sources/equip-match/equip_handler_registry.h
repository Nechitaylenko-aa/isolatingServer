// equip/equip_handler_registry.h
#pragma once

#include <memory>
#include <unordered_map>

#include "equip_match_handler.h" // IEquipMatchHandler
#include "../include/CDatabaseModel.h"

// РЕШЕНИЕ (отклонение от черновика architecture.md §4): SEquipTypeKeyHash
// здесь НЕ переобъявляется. groupEquipRowsByType() (Группа 3,
// equip_row_grouping.h) уже использует std::unordered_map<SEquipTypeKey, ...,
// SEquipTypeKeyHash>, а значит структура хеша определена в database_model.h
// вместе с самим SEquipTypeKey. Повторное определение в этом файле привело
// бы к конфликту при совместном включении equip_row_grouping.h и
// equip_handler_registry.h в equip_query_dispatcher.h.

class CEquipHandlerRegistry
{
public:
    void registerHandler(SEquipTypeKey key, std::unique_ptr<IEquipMatchHandler> handler);
    IEquipMatchHandler* find(const SEquipTypeKey& key) const; // nullptr, если не зарегистрирован

private:
    std::unordered_map<SEquipTypeKey, std::unique_ptr<IEquipMatchHandler>, SEquipTypeKeyHash> m_handlers;
};
