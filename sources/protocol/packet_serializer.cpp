// protocol/packet_serializer.cpp
#include "packet_serializer.h"

#include <algorithm>
#include <cstring>
#include <ctime>

#include "../logger-common/limits.h"
#include "../logger-common/logger.h"

// ---------------------------------------------------------------------------
// Вспомогательные функции
// ---------------------------------------------------------------------------
namespace {

void appendRaw(std::vector<uint8_t>& buf, const void* data, size_t size)
{
    const auto* ptr = static_cast<const uint8_t*>(data);
    buf.insert(buf.end(), ptr, ptr + size);
}

template<typename T>
void appendPod(std::vector<uint8_t>& buf, const T& value)
{
    appendRaw(buf, &value, sizeof(T));
}

} // namespace

// ---------------------------------------------------------------------------

CPacketSerializer::CPacketSerializer(IPacketCipher& cipher)
    : m_cipher(cipher)
{}

// ---------------------------------------------------------------------------

SClientPacket CPacketSerializer::buildResponseHeader(const SClientPacket& req,
                                                     EResponseStatus      status,
                                                     uint32_t             payloadLength) const
{
    SClientPacket resp{};
    resp.magic        = PROTOCOL_MAGIC;
    resp.version      = req.version;
    resp.query_type   = req.query_type;
    resp.id_client    = req.id_client;
    resp.length       = payloadLength;
    resp.timestamp    = static_cast<uint64_t>(std::time(nullptr));
    resp.keyType      = ESignatureKeyType::KT_NONE;
    resp.is_signature = 0;
    resp.status       = status;
    return resp;
}

std::vector<uint8_t> CPacketSerializer::finalize(const SClientPacket&        respHeader,
                                                  const std::vector<uint8_t>& payload,
                                                  bool                        encryptResponse) const
{
    // Собираем: SClientPacket + payload
    std::vector<uint8_t> body;
    body.reserve(sizeof(SClientPacket) + payload.size());
    appendRaw(body, &respHeader, sizeof(SClientPacket));
    body.insert(body.end(), payload.begin(), payload.end());

    if (encryptResponse)
    {
        if (m_cipher.isAvailable())
        {
            auto encrypted = m_cipher.encrypt(body.data(), body.size());
            std::vector<uint8_t> result;
            result.reserve(1 + encrypted.size());
            result.push_back(0x01);
            result.insert(result.end(), encrypted.begin(), encrypted.end());
            return result;
        }
        CLogger::instance().warn("CPacketSerializer",
            "запрошено шифрование ответа, но cipher недоступен — ответ отправляется незашифрованным");
    }

    std::vector<uint8_t> result;
    result.reserve(1 + body.size());
    result.push_back(0x00);
    result.insert(result.end(), body.begin(), body.end());
    return result;
}

// ---------------------------------------------------------------------------

std::vector<uint8_t> CPacketSerializer::serializeError(const SClientPacket& reqHeader,
                                                        EResponseStatus      status,
                                                        bool                 encryptResponse) const
{
    auto hdr = buildResponseHeader(reqHeader, status, 0);
    return finalize(hdr, {}, encryptResponse);
}

// ---------------------------------------------------------------------------

std::vector<uint8_t> CPacketSerializer::serializeEquipReqResponse(
    const SClientPacket&                 reqHeader,
    const std::vector<SEquipReqRespBlock>& blocks,
    bool                                  encryptResponse) const
{
    std::vector<uint8_t> payload;

    for (const auto& block : blocks)
    {
        appendRaw(payload, &block.header, sizeof(SReqHeader));
        for (const auto& item : block.items)
        {
            appendRaw(payload, &item.meta, sizeof(SResponseEquip));
            if (!item.params.empty())
                appendRaw(payload, item.params.data(), item.params.size() * sizeof(float));
        }
    }

    auto hdr = buildResponseHeader(reqHeader, ERS_SUCCESS, static_cast<uint32_t>(payload.size()));
    return finalize(hdr, payload, encryptResponse);
}

// ---------------------------------------------------------------------------

std::vector<uint8_t> CPacketSerializer::serializeEquipIdResponse(
    const SClientPacket&                  reqHeader,
    const std::vector<SEquipProxyRespItem>& items,
    bool                                   encryptResponse) const
{
    std::vector<uint8_t> payload;

    uint16_t count = static_cast<uint16_t>(items.size());
    appendPod(payload, count);

    for (const auto& item : items)
    {
        appendRaw(payload, &item.meta, sizeof(SEquipProxy));
        if (!item.params.empty())
            appendRaw(payload, item.params.data(), item.params.size() * sizeof(float));
    }

    auto hdr = buildResponseHeader(reqHeader, ERS_SUCCESS, static_cast<uint32_t>(payload.size()));
    return finalize(hdr, payload, encryptResponse);
}

// ---------------------------------------------------------------------------

std::vector<uint8_t> CPacketSerializer::serializeOrgsResponse(
    const SClientPacket&         reqHeader,
    const std::vector<SOrgProxy>& orgs,
    bool                          encryptResponse) const
{
    std::vector<uint8_t> payload;

    uint16_t count = static_cast<uint16_t>(orgs.size());
    appendPod(payload, count);

    for (const auto& org : orgs)
        appendRaw(payload, &org, sizeof(SOrgProxy));

    auto hdr = buildResponseHeader(reqHeader, ERS_SUCCESS, static_cast<uint32_t>(payload.size()));
    return finalize(hdr, payload, encryptResponse);
}

// ---------------------------------------------------------------------------

void CPacketSerializer::copyFixedString(char* dst, size_t dstSize, const std::string& src)
{
    if (dstSize == 0)
        return;

    if (src.size() >= dstSize)
    {
        CLogger::instance().warn("CPacketSerializer",
            "строка '{}' обрезана с {} до {} байт", src, src.size(), dstSize - 1);
        std::memcpy(dst, src.data(), dstSize - 1);
        dst[dstSize - 1] = '\0';
    }
    else
    {
        std::memcpy(dst, src.data(), src.size());
        std::memset(dst + src.size(), 0, dstSize - src.size());
    }
}
