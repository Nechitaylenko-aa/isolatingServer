#include "hash.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace cpptool
{

namespace
{

constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
constexpr uint64_t kFnvPrime = 0x00000100000001b3ULL;

uint64_t fnv1a_update(uint64_t hash, const char* data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        hash ^= static_cast<unsigned char>(data[i]);
        hash *= kFnvPrime;
    }
    return hash;
}

std::string to_hex(uint64_t value)
{
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(value));
    return std::string(buf);
}

} // namespace

uint64_t fnv1a_64(const std::string& data)
{
    return fnv1a_update(kFnvOffsetBasis, data.data(), data.size());
}

std::string hash_string(const std::string& data)
{
    return to_hex(fnv1a_64(data));
}

std::string hash_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    // rdbuf() может выставить failbit на пустом файле — это не ошибка.
    if (in.bad())
        return "";
    return to_hex(fnv1a_64(ss.str()));
}

std::string hash_flags(const std::vector<std::string>& flags)
{
    uint64_t hash = kFnvOffsetBasis;
    for (const auto& flag : flags)
    {
        hash = fnv1a_update(hash, flag.data(), flag.size());
        const char separator = '\0';
        hash = fnv1a_update(hash, &separator, 1);
    }
    return to_hex(hash);
}

} // namespace cpptool
