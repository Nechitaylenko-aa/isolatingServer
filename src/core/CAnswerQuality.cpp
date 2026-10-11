/**
 * @file CAnswerQuality.cpp
 * @brief Parsing of model-estimated answer risk.
 */
#include "core/CAnswerQuality.h"

#include <cstdint>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace fcpp
{

bool parse_answer_quality(const std::string& text, SAnswerQualityAssessment& out)
{
    out = SAnswerQualityAssessment{};
    try
    {
        // The reviewer contract is strict JSON, not a JSON fragment embedded in prose.
        // Any wrapper text or second object is a failed review and must fail closed.
        const nlohmann::json document = nlohmann::json::parse(text);
        if (!document.is_object() || document.size() != 3
                || !document.contains("risk_score")
                || !document["risk_score"].is_number_integer()
                || !document.contains("verdict") || !document["verdict"].is_string()
                || !document.contains("reason") || !document["reason"].is_string())
        {
            return false;
        }

        const std::int64_t risk = document["risk_score"].get<std::int64_t>();
        if (risk < 0 || risk > 100)
        {
            return false;
        }
        const std::string verdict = document["verdict"].get<std::string>();
        if (verdict != "ACCEPT" && verdict != "ESCALATE")
        {
            return false;
        }
        const std::string reason = document["reason"].get<std::string>();
        if (reason.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            return false;
        }

        SAnswerQualityAssessment assessment;
        assessment.m_valid = true;
        assessment.m_risk_score = static_cast<std::uint32_t>(risk);
        assessment.m_escalate = verdict != "ACCEPT" || risk >= 35;
        assessment.m_reason = reason;
        out = std::move(assessment);
        return true;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
}
} // namespace fcpp
