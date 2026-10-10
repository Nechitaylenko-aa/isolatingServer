/**
 * @file CSubtaskOrchestrator.cpp
 * @brief Реализация оркестратора подзадач (ADR-16).
 */

#include "core/CSubtaskOrchestrator.h"

#include <cstddef>
#include <string>

#include "core/CEvidenceCollector.h"
#include "core/CRequestPipeline.h"
#include "fcpp-agent/SRequestOutcome.h"

namespace fcpp
{

CSubtaskOrchestrator::CSubtaskOrchestrator(const CConfigManager& config_manager,
                                           CProjectManager& project_manager)
    : m_config_manager(config_manager)
    , m_project_manager(project_manager)
{
}

bool CSubtaskOrchestrator::run(const SSubtaskPlan& plan,
                               SEvidence& out_evidence,
                               std::vector<std::string>& out_notes) const
{
    out_evidence = SEvidence{};
    out_notes.clear();

    CRequestPipeline pipeline(&m_project_manager, &m_config_manager);
    const CEvidenceCollector collector(m_config_manager, m_project_manager);

    std::size_t index = 0;
    bool produced = false;
    for (const SSubTask& subtask : plan.m_subtasks)
    {
        ++index;
        const std::string prefix = "шаг " + std::to_string(index) + ": ";

        const SRequestOutcome outcome = pipeline.run_from_intent(subtask.m_intent);
        if (outcome.m_kind != EOutcomeKind::Ok)
        {
            // Шаг не разобран — фиксируем видимо (§59), продолжаем остальные.
            out_notes.push_back(prefix + outcome_kind_name(outcome.m_kind) + ": " + outcome.m_message);
            continue;
        }

        SEvidence step_evidence;
        if (!collector.collect(subtask.m_intent, outcome.m_plan, step_evidence))
        {
            out_notes.push_back(prefix + "нет данных");
            continue;
        }

        for (const SEvidenceSection& section : step_evidence.m_sections)
        {
            SEvidenceSection merged = section;
            merged.m_title = prefix + section.m_title;
            out_evidence.m_sections.push_back(std::move(merged));
        }
        out_notes.push_back(prefix + "ok");
        produced = true;
    }

    return produced;
}

} // namespace fcpp
