// protocol/parsed_types.h
#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "../logger-common/limits.h"
#include "../logger-common/nc_component_types.h"

struct SComponentReq
{
    uint16_t               id_component{0};
    uint16_t               componentType{0};
    NCore::EComponentTypes natureType{NCore::EComponentTypes::ect_water};
    std::vector<float>     parameters;
};

struct SEquipIdRequest { std::vector<uint16_t> ids;   };   // EPT_EQIP_ID
struct SOrgTypeRequest { std::vector<uint16_t> types; };   // EPT_ORGS_REQ
struct SOrgIdRequest   { std::vector<uint32_t> ids;   };   // EPT_ORGS_ID

using TParsedPayload = std::variant<
    std::vector<SComponentReq>,  // EPT_EQUIP_REQ
    SEquipIdRequest,              // EPT_EQIP_ID
    SOrgTypeRequest,              // EPT_ORGS_REQ
    SOrgIdRequest                 // EPT_ORGS_ID
>;
