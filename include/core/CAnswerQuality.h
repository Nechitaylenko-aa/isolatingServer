/**
 * @file CAnswerQuality.h
 * @brief Parsing and threshold policy for model-estimated answer risk.
 */
#ifndef FCPP_AGENT_CANSWERQUALITY_H
#define FCPP_AGENT_CANSWERQUALITY_H

#include <cstdint>
#include <string>

namespace fcpp
{

/** Model-estimated risk, not a calibrated probability of correctness. */
struct SAnswerQualityAssessment
{
    bool m_valid = false;
    std::uint32_t m_risk_score = 100;
    bool m_escalate = true;
    std::string m_reason;
};

/** Parse the strict JSON response from the answer reviewer. */
bool parse_answer_quality(const std::string& text, SAnswerQualityAssessment& out);

} // namespace fcpp

#endif // FCPP_AGENT_CANSWERQUALITY_H
