/**
 * @file SStaleReport.cpp
 * @brief Реализация имён причин устаревания индекса (Секция 3 rules.md).
 */

#include "fcpp-agent/SStaleReport.h"

#include <cstddef>

namespace fcpp
{

namespace
{

/** @brief Имена причин устаревания, индексированные по EStaleReason. */
static const char* const m_stale_reason_names[static_cast<std::size_t>(EStaleReason::Count)]
{
    "None",
    "EnvironmentChange",
    "ContentChange",
    "Missing"
};

} // namespace

const char* stale_reason_name(EStaleReason reason)
{
    return m_stale_reason_names[static_cast<std::size_t>(reason)];
}

} // namespace fcpp
