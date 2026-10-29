#include "core/CHistorian.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace fcpp
{
namespace
{
std::string normalize_answer(std::string answer)
{
    const auto first = answer.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }

    const auto last = answer.find_last_not_of(" \t\r\n");
    answer = answer.substr(first, last - first + 1);
    std::transform(answer.begin(), answer.end(), answer.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::toupper(character));
                   });
    return answer;
}

bool decode_yes_no(const std::string& text, bool& value, std::string& error)
{
    const std::string answer = normalize_answer(text);
    if (answer == "YES")
    {
        value = true;
        return true;
    }
    if (answer == "NO")
    {
        value = false;
        return true;
    }

    error = "expected exactly YES or NO";
    return false;
}
} // namespace

CHistorian::CHistorian(CLlmLayer& llm)
    : m_llm(llm)
{
}

SHistoryRelevance CHistorian::assess_relevance(
    const std::string& current_request,
    const std::vector<SConversationTurn>& history) const
{
    SHistoryRelevance result;
    if (history.empty())
    {
        result.m_ok = true;
        return result;
    }
    if (current_request.empty())
    {
        result.m_error = "current request is empty";
        return result;
    }

    std::string history_text;
    for (const SConversationTurn& turn : history)
    {
        history_text += "USER: " + turn.m_user_text
            + "\nASSISTANT: " + turn.m_assistant_text + "\n";
    }

    const std::string prompt =
        "Determine whether the current user request cannot be understood correctly "
        "without prior conversation. Answer YES only when a prior turn is necessary "
        "to resolve a reference, continue a task, or recover an explicit constraint "
        "or decision. If the request is independently understandable, answer NO. "
        "Return exactly YES or NO.";
    const std::string input =
        "CURRENT REQUEST:\n" + current_request
        + "\n\nPRIOR CONVERSATION:\n" + history_text;

    const auto decision = m_llm.request<bool>(
        ELlmRole::ThinkerLocal, prompt, input, decode_yes_no);

    result.m_ok = decision.m_ok;
    result.m_history_needed = decision.m_ok && decision.m_value;
    result.m_escalated = decision.m_escalated;
    result.m_used_role = decision.m_used_role;
    result.m_error = decision.m_error;
    return result;
}
} // namespace fcpp
