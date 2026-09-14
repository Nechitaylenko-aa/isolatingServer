// protocol/header_validator.h
#pragma once

#include <cstdint>

#include "active_client_registry.h"
#include "signature_verifier.h"
#include "time_check_registry.h"
#include "../logger-common/wire_types.h"

enum class EValidationOutcome : uint8_t
{
    OK,
    RESPOND_WITH_ERROR,
    DROP_SILENTLY
};

struct SValidationResult
{
    EValidationOutcome outcome{EValidationOutcome::DROP_SILENTLY};
    EResponseStatus    status{ERS_INTERNAL_ERROR}; // значимо только при RESPOND_WITH_ERROR
};

class CHeaderValidator
{
public:
    CHeaderValidator(IActiveClientRegistry& activeClients,
                     CTimeCheckRegistry&    timeCheck,
                     ISignatureVerifier&    sigVerifier);

    // totalMsgSize — размер workBuf (после расшифровки): sizeof(SClientPacket) + payload + хвост подписи
    // wasEncrypted — исходный encrypted_flag (для проверки шага 5)
    SValidationResult validate(const SClientPacket& header,
                               size_t               totalMsgSize,
                               bool                 wasEncrypted) const;

private:
    IActiveClientRegistry& m_activeClients;
    CTimeCheckRegistry&    m_timeCheck;
    ISignatureVerifier&    m_sigVerifier;
};
