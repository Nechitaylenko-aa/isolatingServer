/**
 * @file CProjectPaths.cpp
 * @brief Реализация единого резолвера путей проекта.
 */
#include "core/CProjectPaths.h"

namespace fcpp
{

std::filesystem::path CProjectPaths::resolve(const std::filesystem::path& value,
                                             const std::filesystem::path& base)
{
    if (value.empty())
    {
        return {};
    }
    if (value.is_absolute())
    {
        return value.lexically_normal();
    }
    if (base.empty() || !base.is_absolute())
    {
        return {};
    }
    return (base / value).lexically_normal();
}

} // namespace fcpp
