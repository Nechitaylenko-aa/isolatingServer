/**
 * @file CSubtaskOrchestrator.h
 * @brief Оркестрация подзадач: последовательный прогон + накопление Evidence (ADR-16, §48).
 *
 * Не меняет основной pipeline: каждая подзадача проходит существующие примитивы
 * (CRequestPipeline::run_from_intent + CEvidenceCollector) по одному шагу за раз.
 * Порядок детерминирован; LLM в оркестрации не участвует.
 */

#ifndef FCPP_AGENT_CSUBTASKORCHESTRATOR_H
#define FCPP_AGENT_CSUBTASKORCHESTRATOR_H

#include <string>
#include <vector>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SEvidence.h"
#include "fcpp-agent/SSubtasks.h"

namespace fcpp
{

/**
 * @brief Оркестратор подзадач.
 */
class CSubtaskOrchestrator
{
public:
    /**
     * @brief Конструктор.
     * @param config_manager Конфигурация.
     * @param project_manager Менеджер проектов.
     */
    CSubtaskOrchestrator(const CConfigManager& config_manager, CProjectManager& project_manager);

    /**
     * @brief Деструктор.
     */
    ~CSubtaskOrchestrator() = default;

    /**
     * @brief Выполнить план подзадач, накапливая Evidence.
     * @param plan План декомпозиции.
     * @param out_evidence Накопленный Evidence.
     * @param out_notes Пометки по каждому шагу (видимость, §59).
     * @return true если хотя бы один шаг дал данные.
     */
    bool run(const SSubtaskPlan& plan,
             SEvidence& out_evidence,
             std::vector<std::string>& out_notes) const;

private:
    const CConfigManager& m_config_manager; ///< Конфигурация.
    CProjectManager& m_project_manager;     ///< Менеджер проектов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CSUBTASKORCHESTRATOR_H
