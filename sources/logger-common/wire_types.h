// protocol/wire_types.h
//
// Только POD-структуры/enum'ы бинарного протокола. Никакой валидации и
// (де)сериализации здесь нет — это Группа 2. Взято дословно из protocol.md,
// с единственной содержательной правкой: NCore::EComponentTypes зафиксирован
// как uint8_t (см. nc_component_types.h).

#pragma once

#include <cstdint>

#include "../../../interfaces/include/core-types.h"

#pragma pack(push, 1)

enum EQueryType : uint8_t
{
    EPT_EQUIP_REQ,
    EPT_ORGS_REQ,
    EPT_EQIP_ID,
    EPT_ORGS_ID,
    EPT_COUNT
};

enum class ESignatureKeyType : uint8_t
{
    KT_NONE,
    KT_RSA_1024,
    KT_RSA_2048,
    KT_RSA_4096,
    KT_COUNT
};

enum EResponseStatus : uint16_t
{
    ERS_SUCCESS = 0,
    ERS_INVALID_MAGIC,
    ERS_UNSUPPORTED_VERSION,
    ERS_INVALID_QUERY_TYPE,
    ERS_INVALID_LENGTH,
    ERS_INVALID_SIGNATURE,
    ERS_ENCRYPTION_ERROR,
    ERS_NOT_FOUND,
    ERS_INTERNAL_ERROR,
    ERS_DATABASE_QUERY_ERROR,
    ERS_COUNT
};

struct SClientPacket
{
    uint32_t            magic{0};
    uint16_t            version{0};
    EQueryType          query_type{EPT_COUNT};
    uint16_t            id_client{0};
    uint32_t            length{0};
    uint64_t            timestamp{0};
    ESignatureKeyType    keyType{ESignatureKeyType::KT_NONE};
    uint8_t              is_signature{0};
    EResponseStatus      status{ERS_INVALID_MAGIC};
};
static_assert(sizeof(SClientPacket) == 25,
              "SClientPacket: неожиданный размер под pack(1) - проверьте порядок/типы полей");

struct SSignature
{
    uint32_t hash{0};   // CRC-32 от SClientPacket + payload (незашифрованных)
    uint32_t length{0}; // дубликат SClientPacket::length
};
static_assert(sizeof(SSignature) == 8, "SSignature: неожиданный размер под pack(1)");

// --- payload-структура запроса подбора оборудования ---
struct SEquipRequestData
{
    uint16_t                 id_component{0};
    uint16_t                 componentType{0};
    NCore::EComponentTypes   natureType{NCore::EComponentTypes::ect_water}; // uint8_t, см. nc_component_types.h
    uint16_t                 param_amount{0};
    // далее следует param_amount штук float — вне структуры, читается отдельно на этапе парсинга (Группа 2)
};
static_assert(sizeof(SEquipRequestData) == 7, "SEquipRequestData: неожиданный размер под pack(1)");

// --- payload-структуры ответа подбора оборудования ---
struct SReqHeader
{
    uint16_t component_id{0};
    uint16_t response_amount{0};
};
static_assert(sizeof(SReqHeader) == 4, "SReqHeader: неожиданный размер под pack(1)");

struct SResponseEquip
{
    uint16_t equip_id{0};
    char     name[30];
    uint16_t param_amount{0};
};
static_assert(sizeof(SResponseEquip) == 34, "SResponseEquip: неожиданный размер под pack(1)");

// --- payload EPT_EQIP_ID ответ ---
struct SEquipProxy
{
    uint16_t id{0};
    uint16_t id_manufacturer{0};
    char     equip_name[30];
    char     manufacturer[30];
    uint16_t param_amount{0};
};
static_assert(sizeof(SEquipProxy) == 66, "SEquipProxy: неожиданный размер под pack(1)");

// --- payload EPT_ORGS_REQ / EPT_ORGS_ID ответ ---
struct SOrgProxy
{
    uint32_t id{0};
    char     name[30];
    uint16_t id_type{0};
    uint16_t id_ownership{0};
    uint16_t id_country{0};
    char     inn[12];
};
static_assert(sizeof(SOrgProxy) == 52, "SOrgProxy: неожиданный размер под pack(1)");

#pragma pack(pop)
