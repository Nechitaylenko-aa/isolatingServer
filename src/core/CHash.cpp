/**
 * @file CHash.cpp
 * @brief Реализация встроенного SHA-256 (FIPS 180-4).
 */

#include "core/CHash.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace fcpp
{

namespace
{

/** @brief Раундовые константы SHA-256. */
constexpr std::uint32_t k_round_constants[64] =
{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

/**
 * @brief Циклический сдвиг вправо на 32 битах.
 * @param value Исходное значение.
 * @param count Величина сдвига.
 * @return Результат циклического сдвига.
 */
std::uint32_t rotate_right(std::uint32_t value, std::uint32_t count)
{
    return (value >> count) | (value << (32u - count));
}

/**
 * @brief Прочитать 32-битное слово в big-endian.
 * @param data Указатель на 4 байта.
 * @return Значение слова.
 */
std::uint32_t read_be32(const unsigned char* data)
{
    return (static_cast<std::uint32_t>(data[0]) << 24u)
           | (static_cast<std::uint32_t>(data[1]) << 16u)
           | (static_cast<std::uint32_t>(data[2]) << 8u)
           | static_cast<std::uint32_t>(data[3]);
}

/**
 * @brief Обработать один 64-байтовый блок.
 * @param block Блок данных.
 * @param state Текущее состояние хэша (8 слов).
 */
void transform_block(const unsigned char* block, std::uint32_t* state)
{
    std::uint32_t schedule[64];

    for (std::uint32_t index = 0; index < 16u; ++index)
    {
        schedule[index] = read_be32(block + (index * 4u));
    }

    for (std::uint32_t index = 16u; index < 64u; ++index)
    {
        const std::uint32_t s0 = rotate_right(schedule[index - 15u], 7u)
                                 ^ rotate_right(schedule[index - 15u], 18u)
                                 ^ (schedule[index - 15u] >> 3u);
        const std::uint32_t s1 = rotate_right(schedule[index - 2u], 17u)
                                 ^ rotate_right(schedule[index - 2u], 19u)
                                 ^ (schedule[index - 2u] >> 10u);
        schedule[index] = schedule[index - 16u] + s0 + schedule[index - 7u] + s1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];

    for (std::uint32_t index = 0; index < 64u; ++index)
    {
        const std::uint32_t sigma1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
        const std::uint32_t choose = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 = h + sigma1 + choose + k_round_constants[index] + schedule[index];
        const std::uint32_t sigma0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = sigma0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

} // namespace

std::string CHash::sha256_hex(const std::string& text)
{
    std::uint32_t state[8] =
    {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    const unsigned char* data = reinterpret_cast<const unsigned char*>(text.data());
    const std::size_t length = text.size();

    std::size_t offset = 0;
    while (offset + 64u <= length)
    {
        transform_block(data + offset, state);
        offset += 64u;
    }

    unsigned char tail[128];
    const std::size_t remaining = length - offset;
    if (remaining > 0)
    {
        std::memcpy(tail, data + offset, remaining);
    }
    tail[remaining] = 0x80u;

    const std::size_t total = ((remaining + 1u + 8u + 63u) / 64u) * 64u;
    for (std::size_t index = remaining + 1u; index < total; ++index)
    {
        tail[index] = 0u;
    }

    const std::uint64_t bit_length = static_cast<std::uint64_t>(length) * 8u;
    for (std::uint32_t index = 0; index < 8u; ++index)
    {
        tail[total - 1u - index] = static_cast<unsigned char>((bit_length >> (8u * index)) & 0xffu);
    }

    for (std::size_t index = 0; index < total; index += 64u)
    {
        transform_block(tail + index, state);
    }

    static const char* const hex_digits = "0123456789abcdef";
    std::string digest;
    digest.reserve(64);
    for (std::uint32_t word = 0; word < 8u; ++word)
    {
        for (int byte_index = 3; byte_index >= 0; --byte_index)
        {
            const unsigned char byte =
                static_cast<unsigned char>((state[word] >> (8u * static_cast<std::uint32_t>(byte_index))) & 0xffu);
            digest.push_back(hex_digits[byte >> 4u]);
            digest.push_back(hex_digits[byte & 0x0fu]);
        }
    }

    return digest;
}

} // namespace fcpp
