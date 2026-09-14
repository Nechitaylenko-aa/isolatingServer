#pragma once
#include "../equip-match/equip_handler_registry.h"

/** Регистрация конкретных хендлеров подбора (CWaterFilterLightHandler и т.п.) -
 * реализация алгоритмов вне зоны любого ТЗ, только сама регистрация.*/
void registerEquipHandlers(CEquipHandlerRegistry& registry);
