// protocol/packet_parser.h
#pragma once

#include <cstdint>

#include "parsed_types.h"
#include "../logger-common/wire_types.h"

struct SParseOutcome
{
    bool            ok{false};
    EResponseStatus status{ERS_SUCCESS};  // значимо при ok == false
    TParsedPayload  payload;              // значимо при ok == true
};

class CPacketParser
{
public:
    // payload/payloadLen — байты строго после SClientPacket, длиной header.length
    // query_type уже провалидирован CHeaderValidator'ом
    static SParseOutcome parse(EQueryType queryType, const uint8_t* payload, uint32_t payloadLen);

private:
    static SParseOutcome parseEquipReq(const uint8_t* p, uint32_t len);
    static SParseOutcome parseEquipId (const uint8_t* p, uint32_t len);
    static SParseOutcome parseOrgsReq (const uint8_t* p, uint32_t len);
    static SParseOutcome parseOrgsId  (const uint8_t* p, uint32_t len);
};
