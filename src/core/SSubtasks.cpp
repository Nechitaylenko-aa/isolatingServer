/**
 * @file SSubtasks.cpp
 * @brief Реализация методов модели подзадач (ADR-16).
 */

#include "fcpp-agent/SSubtasks.h"

namespace fcpp
{

std::size_t SSubtaskPlan::size() const
{
    return m_subtasks.size();
}

} // namespace fcpp
