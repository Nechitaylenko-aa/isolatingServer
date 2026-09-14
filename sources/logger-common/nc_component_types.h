// protocol/nc_component_types.h
//
// Заглушка внешнего заголовка NCore::EComponentTypes (см. server.md, раздел
// "Структуры из внешних источников"). Группа 1 этим типом не владеет, но
// фиксирует ожидаемый wire-контракт: подлежащий тип — uint8_t (1 байт).
// Это важно для #pragma pack(push,1) и static_assert-ов в wire_types.h.
//
// ВНИМАНИЕ: исходно (server.md) этот enum был объявлен как int8_t.
// По решению — используем uint8_t везде. При интеграции с реальным внешним
// модулем NCore нужно свести оба определения к uint8_t (либо на их
// стороне, либо явным приведением на границе), иначе возможна рассинхронизация
// по знаковости при линковке с настоящей реализацией NCore.

#pragma once

#include "../../../interfaces/include/core-types.h"

namespace NCore
{



} // namespace NCore
