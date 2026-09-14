// protocol/nc_component_types.h
//
// Уже реализован в Группе 1 (см. память/предыдущие поставки). Эта копия
// нужна только для автономной сборки/тестов Группы 3 в изоляции - при
// сборке всего проекта используется настоящий файл из src/protocol/,
// содержимое идентично.

#pragma once

#include <cstdint>

namespace NCore
{
enum EComponentTypes : uint8_t
{
    ect_water,
    ect_gas,
    ect_electric,
    ect_count
};
} // namespace NCore
