/**
 * @file SCascade.cpp
 * @brief Имена уровней и глюков каскада (Секция 3 rules.md).
 */

#include "fcpp-agent/SCascade.h"

#include <cstddef>

namespace fcpp
{

namespace
{

/** @brief Имена уровней, индексированные по EModelLevel. */
static const char* const m_level_names[static_cast<std::size_t>(EModelLevel::Count)]
{
    "LITE", "MAIN", "REMOTE"
};

/** @brief Имена типов глюков, индексированные по EGlitchKind. */
static const char* const m_glitch_names[static_cast<std::size_t>(EGlitchKind::Count)]
{
    "NONE", "SYNTAX_FAIL", "REFERENCE_FAIL", "COMPILATION_FAIL",
    "LOOP_FAIL", "SCHEMA_FAIL", "PROVIDER_ERROR", "EMPTY_REPLY"
};

} // namespace

const char* model_level_name(EModelLevel level)
{
    return m_level_names[static_cast<std::size_t>(level)];
}

const char* glitch_kind_name(EGlitchKind glitch)
{
    return m_glitch_names[static_cast<std::size_t>(glitch)];
}

} // namespace fcpp
