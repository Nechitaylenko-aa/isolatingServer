/**
 * @file CAnswerService.h
 * @brief Answer generation and risk review, separate from request routing.
 */
#ifndef FCPP_AGENT_CANSWERSERVICE_H
#define FCPP_AGENT_CANSWERSERVICE_H

#include <functional>
#include <string>
#include <vector>

#include "core/CAnswerQuality.h"
#include "core/CConfigManager.h"
#include "core/CConversationStore.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SCascade.h"
#include "fcpp-agent/SEvidence.h"

namespace fcpp
{

class CAnswerService
{
public:
    CAnswerService(const CConfigManager& config_manager, CProjectManager& project_manager);

    bool answer_general_engineering(
        const std::string& question,
        const std::vector<SConversationTurn>& history,
        bool stream,
        std::string& out_answer,
        const std::function<bool(const std::string&)>& on_stream_chunk) const;

    bool answer_from_evidence(
        const std::string& question,
        const SEvidence& evidence,
        bool stream,
        std::string& out_answer,
        const std::function<bool(const std::string&)>& on_stream_chunk) const;

    SAnswerQualityAssessment assess_answer_quality(
        EModelLevel candidate_level,
        const std::string& question,
        const std::string& answer,
        const std::string& evidence_context = {}) const;

private:
    const CConfigManager& m_config_manager;
    CProjectManager& m_project_manager;
};

} // namespace fcpp

#endif // FCPP_AGENT_CANSWERSERVICE_H
