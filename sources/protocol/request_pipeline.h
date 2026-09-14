// protocol/request_pipeline.h
#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "active_client_registry.h"
#include "header_validator.h"
#include "packet_cipher.h"
#include "packet_parser.h"
#include "packet_serializer.h"
#include "parsed_types.h"
#include "signature_verifier.h"
#include "time_check_registry.h"
#include "../logger-common/wire_types.h"

enum class EPipelineAction : uint8_t
{
    PROCEED,     // payload разобран, передавать в бизнес-логику
    RESPOND,     // отправить responseBytes и закрыть соединение
    DISCONNECT   // закрыть без ответа
};

struct SPipelineResult
{
    EPipelineAction       action{EPipelineAction::DISCONNECT};
    std::vector<uint8_t>  responseBytes;  // значимо при RESPOND
    SClientPacket         header;          // значимо при PROCEED (и как источник для error-ответов выше)
    TParsedPayload        payload;          // значимо при PROCEED
    bool                  wasEncrypted{false};
};

class CRequestPipeline
{
public:
    CRequestPipeline(IActiveClientRegistry& activeClients,
                     CTimeCheckRegistry&    timeCheck,
                     IPacketCipher&         cipher,
                     ISignatureVerifier&    sigVerifier);

    // rawData/rawLen — весь буфер от сокета, включая первый байт encrypted_flag
    SPipelineResult process(const uint8_t* rawData, size_t rawLen);

private:
    IActiveClientRegistry& m_activeClients;
    CTimeCheckRegistry&    m_timeCheck;
    IPacketCipher&         m_cipher;
    ISignatureVerifier&    m_sigVerifier;

    CHeaderValidator  m_validator;
    CPacketSerializer m_serializer;  // только для сборки error-ответов внутри process()
};
