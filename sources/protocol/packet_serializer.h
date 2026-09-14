// protocol/packet_serializer.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "packet_cipher.h"
#include "../logger-common/wire_types.h"

// Одна единица оборудования в ответе EPT_EQUIP_REQ
struct SEquipRespItem
{
    SResponseEquip     meta;
    std::vector<float> params;
};

// Один SReqHeader + список найденного оборудования
struct SEquipReqRespBlock
{
    SReqHeader                  header;
    std::vector<SEquipRespItem> items;   // items.size() == header.response_amount
};

// Одна единица оборудования в ответе EPT_EQIP_ID
struct SEquipProxyRespItem
{
    SEquipProxy        meta;
    std::vector<float> params;
};

class CPacketSerializer
{
public:
    explicit CPacketSerializer(IPacketCipher& cipher);

    // Ответ с ошибкой: [flag][SClientPacket{status=status, length=0}]
    std::vector<uint8_t> serializeError(const SClientPacket& reqHeader,
                                        EResponseStatus      status,
                                        bool                 encryptResponse) const;

    std::vector<uint8_t> serializeEquipReqResponse(const SClientPacket&                 reqHeader,
                                                   const std::vector<SEquipReqRespBlock>& blocks,
                                                   bool                                  encryptResponse) const;

    std::vector<uint8_t> serializeEquipIdResponse(const SClientPacket&                  reqHeader,
                                                  const std::vector<SEquipProxyRespItem>& items,
                                                  bool                                   encryptResponse) const;

    // EPT_ORGS_REQ и EPT_ORGS_ID — идентичный формат
    std::vector<uint8_t> serializeOrgsResponse(const SClientPacket&         reqHeader,
                                               const std::vector<SOrgProxy>& orgs,
                                               bool                          encryptResponse) const;

    // Копирует src в fixed-size буфер: обрезает по dstSize-1, нулевой хвост.
    // При обрезке — WARN в CLogger.
    static void copyFixedString(char* dst, size_t dstSize, const std::string& src);

private:
    IPacketCipher& m_cipher;

    SClientPacket buildResponseHeader(const SClientPacket& req,
                                      EResponseStatus      status,
                                      uint32_t             payloadLength) const;

    std::vector<uint8_t> finalize(const SClientPacket&        respHeader,
                                  const std::vector<uint8_t>& payload,
                                  bool                        encryptResponse) const;
};
