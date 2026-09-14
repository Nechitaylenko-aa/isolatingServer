// protocol/limits.h

#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint32_t MAX_INCOMING_ENCRYPTED_BYTES   = 500 * 1024;
constexpr uint32_t MAX_INCOMING_UNENCRYPTED_BYTES = 200 * 1024;

// Было uint16_t — 0xDEADBEEF (3 735 928 559) в него не помещается и молча
// усекается до 0xBEEF при copy-init (не list-init, поэтому без ошибки
// компиляции). SClientPacket::magic — uint32_t, так что сравнение
// packet.magic == PROTOCOL_MAGIC было бы всегда false. Исправлено на uint32_t.
constexpr uint32_t PROTOCOL_MAGIC = 0xDEADBEEF;

constexpr uint16_t PROTOCOL_SUPPORTED_VERSION = 1;
constexpr size_t   MAX_ACTIVE_CLIENTS         = 50;
constexpr size_t   TIME_CHECK_REGISTRY_CAPACITY = 100;
