// protocol/packet_cipher.h
#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

class IPacketCipher
{
public:
    virtual ~IPacketCipher() = default;

    virtual bool isAvailable() const noexcept = 0;
    virtual std::optional<std::vector<uint8_t>> decrypt(const uint8_t* data, size_t len) const = 0;
    virtual std::vector<uint8_t> encrypt(const uint8_t* data, size_t len) const = 0;
};

class CNullPacketCipher final : public IPacketCipher
{
public:
    bool isAvailable() const noexcept override { return false; }

    std::optional<std::vector<uint8_t>> decrypt(const uint8_t*, size_t) const override
    {
        return std::nullopt;
    }

    std::vector<uint8_t> encrypt(const uint8_t*, size_t) const override
    {
        throw std::logic_error("CNullPacketCipher::encrypt called while isAvailable() == false");
    }
};
