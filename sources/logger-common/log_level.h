// logger/log_level.h

#pragma once

#include <cstdint>

enum class ELogLevel : uint8_t
{
    LL_TRACE,
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_CRITICAL
};
