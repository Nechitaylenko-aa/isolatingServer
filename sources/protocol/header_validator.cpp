
#include "header_validator.h"
#include "../logger-common/limits.h"


CHeaderValidator::CHeaderValidator(IActiveClientRegistry& activeClients,
                                   CTimeCheckRegistry&    timeCheck,
                                   ISignatureVerifier&    sigVerifier)
    : m_activeClients(activeClients)
    , m_timeCheck(timeCheck)
    , m_sigVerifier(sigVerifier)
{}

SValidationResult CHeaderValidator::validate(const SClientPacket& header,
                                             size_t               totalMsgSize,
                                             bool                 wasEncrypted) const
{
    // 1. Magic
    if (header.magic != PROTOCOL_MAGIC)
        return {EValidationOutcome::RESPOND_WITH_ERROR, ERS_INVALID_MAGIC};

    // 2. Version
    if (header.version != PROTOCOL_SUPPORTED_VERSION)
        return {EValidationOutcome::RESPOND_WITH_ERROR, ERS_UNSUPPORTED_VERSION};

    // 3. Query type
    if (header.query_type >= EPT_COUNT)
        return {EValidationOutcome::RESPOND_WITH_ERROR, ERS_INVALID_QUERY_TYPE};

    // 4. Length coherence
    // tailSize: если is_signature==1, хвост должен быть >= sizeof(SSignature)
    // (точный размер подписи не фиксируется — верификация заглушка)
    const size_t minTail = (header.is_signature != 0) ? sizeof(SSignature) : 0;
    const size_t minTotal = sizeof(SClientPacket) + header.length + minTail;

    if (header.is_signature != 0)
    {
        // totalMsgSize должен быть >= sizeof(SClientPacket) + length + sizeof(SSignature)
        if (totalMsgSize < minTotal)
            return {EValidationOutcome::RESPOND_WITH_ERROR, ERS_INVALID_LENGTH};
    }
    else
    {
        // без подписи — точное совпадение
        if (totalMsgSize != sizeof(SClientPacket) + header.length)
            return {EValidationOutcome::RESPOND_WITH_ERROR, ERS_INVALID_LENGTH};
    }

    // 5. Зашифрован, но подпись отсутствует — нарушение протокола
    if (wasEncrypted && header.is_signature == 0)
        return {EValidationOutcome::RESPOND_WITH_ERROR, ERS_ENCRYPTION_ERROR};

    // 6. id_client уже активен — молчаливый сброс
    /*
    if (m_activeClients.isActive(header.id_client))
        return {EValidationOutcome::DROP_SILENTLY, ERS_INTERNAL_ERROR};
    */

    // 7. Timestamp — anti-replay
    /*
    if (!m_timeCheck.checkAndUpdate(header.id_client, header.timestamp))
        return {EValidationOutcome::DROP_SILENTLY, ERS_INTERNAL_ERROR};
    */
    // 8. Проверка подписи (заглушка, всегда true)
    if (header.is_signature != 0)
    {
        // Определяем указатель на SSignature и хвост подписи.
        // На этом уровне у нас нет сырого буфера — верификация откладывается в CRequestPipeline,
        // где буфер доступен; здесь verify вызывается с нулями-заглушками (заглушка всё равно true).
        SSignature sig{};
        if (!m_sigVerifier.verify(nullptr, 0, sig, nullptr, 0, header.keyType))
            return {EValidationOutcome::RESPOND_WITH_ERROR, ERS_INVALID_SIGNATURE};
    }

    return {EValidationOutcome::OK, ERS_SUCCESS};
}
