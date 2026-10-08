/**
 * @file SRequestOutcome.cpp
 * @brief Имена исходов и стадий конвейера (Секция 3 rules.md).
 */

#include "fcpp-agent/SRequestOutcome.h"

#include <cstddef>

namespace fcpp
{

namespace
{

/** @brief Имена классов исходов, индексированные по EOutcomeKind. */
static const char* const m_outcome_names[static_cast<std::size_t>(EOutcomeKind::Count)]
{
    "OK", "DEGRADE", "CLARIFY", "NOT_UNDERSTOOD", "REFUSED", "FAILED"
};

/** @brief Имена стадий, индексированные по EStage. */
static const char* const m_stage_names[static_cast<std::size_t>(EStage::Count)]
{
    "RECEIVED", "INTERPRETED", "VALIDATED", "RESOLVED", "POLICY_CHECKED", "PLANNED"
};

} // namespace

const char* outcome_kind_name(EOutcomeKind kind)
{
    return m_outcome_names[static_cast<std::size_t>(kind)];
}

const char* stage_name(EStage stage)
{
    return m_stage_names[static_cast<std::size_t>(stage)];
}

} // namespace fcpp
