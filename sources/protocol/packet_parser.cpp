// protocol/packet_parser.cpp
#include "packet_parser.h"


#include <cstring>
#include "../logger-common/logger.h"
// ---------------------------------------------------------------------------
// Вспомогательные функции чтения из сырого буфера без UB
// ---------------------------------------------------------------------------
namespace {

template<typename T>
bool readAt(const uint8_t* buf, uint32_t bufLen, uint32_t offset, T& out)
{
    if (static_cast<uint64_t>(offset) + sizeof(T) > bufLen)
        return false;
    std::memcpy(&out, buf + offset, sizeof(T));
    return true;
}

} // namespace

// ---------------------------------------------------------------------------

SParseOutcome CPacketParser::parse(EQueryType queryType, const uint8_t* payload, uint32_t payloadLen)
{
    switch (queryType)
    {
        case EPT_EQUIP_REQ: return parseEquipReq(payload, payloadLen);
        case EPT_EQIP_ID:   return parseEquipId (payload, payloadLen);
        case EPT_ORGS_REQ:  return parseOrgsReq (payload, payloadLen);
        case EPT_ORGS_ID:   return parseOrgsId  (payload, payloadLen);
        default:
            return {false, ERS_INVALID_QUERY_TYPE, {}};
    }
}

// ---------------------------------------------------------------------------
// EPT_EQUIP_REQ
// ---------------------------------------------------------------------------
SParseOutcome CPacketParser::parseEquipReq(const uint8_t* p, uint32_t len)
{
    std::vector<SComponentReq> result;
    uint32_t offset = 0;

    while (offset < len)
    {
        SEquipRequestData block{};
        if (!readAt(p, len, offset, block))
            return {false, ERS_INVALID_LENGTH, {}};
        offset += static_cast<uint32_t>(sizeof(SEquipRequestData));

        const uint32_t floatBytes = static_cast<uint32_t>(block.param_amount) * sizeof(float); // block.param_amount = 40 (!!!)
        if (len - offset < floatBytes)
            return {false, ERS_INVALID_LENGTH, {}};

        if (block.param_amount > 0)
        {
            SComponentReq req;
            req.id_component = block.id_component;
            req.componentType = block.componentType;
            req.natureType    = block.natureType;
            req.parameters.resize(block.param_amount);
            std::memcpy(req.parameters.data(), p + offset, floatBytes);
            result.push_back(std::move(req));
        }
        // param_amount == 0: блок потреблён, в результат не попадает

        offset += floatBytes;
    }
    CLogger::instance().info("CPacketParser", "parseEquipReq::received {} components queries", result.size());
    return {true, ERS_SUCCESS, std::move(result)};
}

// ---------------------------------------------------------------------------
// EPT_EQIP_ID
// ---------------------------------------------------------------------------
SParseOutcome CPacketParser::parseEquipId(const uint8_t* p, uint32_t len)
{
    uint16_t count = 0;
    if (!readAt(p, len, 0, count))
        return {false, ERS_INVALID_LENGTH, {}};

    const uint32_t expected = sizeof(uint16_t) + static_cast<uint32_t>(count) * sizeof(uint16_t);
    if (len != expected)
        return {false, ERS_INVALID_LENGTH, {}};

    SEquipIdRequest req;
    req.ids.resize(count);
    std::memcpy(req.ids.data(), p + sizeof(uint16_t), count * sizeof(uint16_t));
    return {true, ERS_SUCCESS, std::move(req)};
}

// ---------------------------------------------------------------------------
// EPT_ORGS_REQ
// ---------------------------------------------------------------------------
SParseOutcome CPacketParser::parseOrgsReq(const uint8_t* p, uint32_t len)
{
    uint16_t count = 0;
    if (!readAt(p, len, 0, count))
        return {false, ERS_INVALID_LENGTH, {}};

    const uint32_t expected = sizeof(uint16_t) + static_cast<uint32_t>(count) * sizeof(uint16_t);
    if (len != expected)
        return {false, ERS_INVALID_LENGTH, {}};

    SOrgTypeRequest req;
    req.types.resize(count);
    std::memcpy(req.types.data(), p + sizeof(uint16_t), count * sizeof(uint16_t));
    return {true, ERS_SUCCESS, std::move(req)};
}

// ---------------------------------------------------------------------------
// EPT_ORGS_ID
// ---------------------------------------------------------------------------
SParseOutcome CPacketParser::parseOrgsId(const uint8_t* p, uint32_t len)
{
    uint16_t count = 0;
    if (!readAt(p, len, 0, count))
        return {false, ERS_INVALID_LENGTH, {}};

    const uint32_t expected = sizeof(uint16_t) + static_cast<uint32_t>(count) * sizeof(uint32_t);
    if (len != expected)
        return {false, ERS_INVALID_LENGTH, {}};

    SOrgIdRequest req;
    req.ids.resize(count);
    std::memcpy(req.ids.data(), p + sizeof(uint16_t), count * sizeof(uint32_t));
    return {true, ERS_SUCCESS, std::move(req)};
}
