/**
 * @file SError.cpp
 * @brief Реализация типизированных ошибок (Секция 3 rules.md).
 */

#include "fcpp-agent/SError.h"

#include <cstddef>

namespace fcpp
{

namespace
{

/** @brief Имена категорий ошибок, индексированные по EErrorKind. */
static const char* const m_error_kind_names[static_cast<std::size_t>(EErrorKind::Count)]
{
    "Ok",
    "InvalidRequest",
    "SymbolNotFound",
    "Ambiguous",
    "NotUnderstood",
    "IndexNotReady",
    "IndexStale",
    "IoError",
    "ProjectNotOpen",
    "PermissionDenied",
    "PatchInvalid",
    "PatchApplyFailed",
    "BuildFailed"
};

} // namespace

const char* error_kind_name(EErrorKind kind)
{
    return m_error_kind_names[static_cast<std::size_t>(kind)];
}

} // namespace fcpp
