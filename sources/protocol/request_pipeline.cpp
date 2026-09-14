// protocol/request_pipeline.cpp
#include "request_pipeline.h"

#include <cstring>



CRequestPipeline::CRequestPipeline(IActiveClientRegistry& activeClients,
                                   CTimeCheckRegistry&    timeCheck,
                                   IPacketCipher&         cipher,
                                   ISignatureVerifier&    sigVerifier)
    : m_activeClients(activeClients)
    , m_timeCheck(timeCheck)
    , m_cipher(cipher)
    , m_sigVerifier(sigVerifier)
    , m_validator(activeClients, timeCheck, sigVerifier)
    , m_serializer(cipher)
{}

SPipelineResult CRequestPipeline::process(const uint8_t* rawData, size_t rawLen)
{
    // --- 1. Минимальная длина (хотя бы флаг)
    if (rawLen < 1)
        return {};  // DISCONNECT

    const uint8_t flag = rawData[0];

    if (flag != 0x00 && flag != 0x01)
        return {};  // неизвестный флаг — DISCONNECT

    const bool   wasEncrypted = (flag == 0x01);
    const uint8_t* body       = rawData + 1;
    const size_t   bodyLen    = rawLen - 1;

    // --- 2. Лимит размера ДО расшифровки (защита от DoS)
    const size_t limit = wasEncrypted ? MAX_INCOMING_ENCRYPTED_BYTES
                                       : MAX_INCOMING_UNENCRYPTED_BYTES;
    if (bodyLen > limit)
        return {};  // DISCONNECT, без ответа

    // --- 3. Расшифровка (если нужна)
    std::vector<uint8_t> decryptedBuf;
    const uint8_t* workPtr = nullptr;
    size_t         workLen = 0;

    if (wasEncrypted)
    {
        auto result = m_cipher.decrypt(body, bodyLen);
        if (!result.has_value())
            return {};  // DISCONNECT — нет заголовка, отвечать нечем
        decryptedBuf = std::move(*result);
        workPtr = decryptedBuf.data();
        workLen = decryptedBuf.size();
    }
    else
    {
        workPtr = body;
        workLen = bodyLen;
    }

    // --- 4. Минимум на SClientPacket
    if (workLen < sizeof(SClientPacket))
        return {};  // DISCONNECT

    SClientPacket header{};
    std::memcpy(&header, workPtr, sizeof(SClientPacket));

    // --- 5. Валидация заголовка
    const SValidationResult validation = m_validator.validate(header, workLen, wasEncrypted);

    if (validation.outcome == EValidationOutcome::DROP_SILENTLY)
        return {};  // DISCONNECT

    if (validation.outcome == EValidationOutcome::RESPOND_WITH_ERROR)
    {
        SPipelineResult res;
        res.action        = EPipelineAction::RESPOND;
        res.responseBytes = m_serializer.serializeError(header, validation.status, wasEncrypted);
        res.header        = header;
        res.wasEncrypted = flag;
        return res;
    }

    // --- 6. Парсинг payload
    const uint8_t* payloadPtr = workPtr + sizeof(SClientPacket);
    const SParseOutcome parsed = CPacketParser::parse(header.query_type, payloadPtr, header.length);

    if (!parsed.ok)
    {
        SPipelineResult res;
        res.action        = EPipelineAction::RESPOND;
        res.responseBytes = m_serializer.serializeError(header, parsed.status, wasEncrypted);
        res.header        = header;
        res.wasEncrypted = flag;
        return res;
    }

    // --- 7. Всё хорошо
    SPipelineResult res;
    res.action  = EPipelineAction::PROCEED;
    res.header  = header;
    res.payload = parsed.payload;
    res.wasEncrypted = flag;
    return res;
}
