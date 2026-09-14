// protocol/signature_verifier.h
#pragma once

#include <cstddef>
#include <cstdint>

#include "../logger-common/wire_types.h"

class ISignatureVerifier
{
public:
    virtual ~ISignatureVerifier() = default;

    // messageBytes — SClientPacket + payload (незашифрованные, без SSignature и хвоста подписи)
    virtual bool verify(const uint8_t* messageBytes, size_t messageLen,
                        const SSignature& sig,
                        const uint8_t* sigBytes, size_t sigLen,
                        ESignatureKeyType keyType) const = 0;
};

// "Подпись если есть, игнорируется. Она на будущее, нужна заглушка" — protocol.md
class CNullSignatureVerifier final : public ISignatureVerifier
{
public:
    bool verify(const uint8_t*, size_t, const SSignature&,
                const uint8_t*, size_t, ESignatureKeyType) const override
    {
        return true;
    }
};
