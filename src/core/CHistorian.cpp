#include "core/CHistorian.h"

#include <algorithm>
#include <limits>
#include <string>

namespace fcpp
{
namespace
{
std::string trim_ascii(const std::string& text)
{
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}
} // namespace

CHistorian::CHistorian(CLlmLayer& llm)
    : m_llm(llm)
{
}

bool CHistorian::decode_relevant_indices(
    const std::string& text, std::size_t turn_count,
    std::vector<std::size_t>& indices, std::string& error)
{
    const std::string normalized = trim_ascii(text);
    if (normalized == "NONE")
    {
        indices.clear();
        return true;
    }
    if (normalized.empty())
    {
        error = "expected NONE or comma-separated 1-based turn numbers";
        return false;
    }

    std::vector<std::size_t> decoded;
    std::size_t position = 0;
    while (position < normalized.size())
    {
        while (position < normalized.size()
               && (normalized[position] == ' ' || normalized[position] == '\t'))
            ++position;
        if (position == normalized.size() || normalized[position] < '0'
            || normalized[position] > '9')
        {
            error = "expected a positive turn number";
            return false;
        }

        std::size_t number = 0;
        while (position < normalized.size()
               && normalized[position] >= '0' && normalized[position] <= '9')
        {
            const std::size_t digit = static_cast<std::size_t>(normalized[position] - '0');
            if (number > (std::numeric_limits<std::size_t>::max() - digit) / 10)
            {
                error = "turn number overflows";
                return false;
            }
            number = number * 10 + digit;
            ++position;
        }
        if (number == 0 || number > turn_count)
        {
            error = "turn number is outside the supplied history";
            return false;
        }

        const std::size_t index = number - 1;
        if (std::find(decoded.begin(), decoded.end(), index) != decoded.end())
        {
            error = "duplicate turn number";
            return false;
        }
        decoded.push_back(index);

        while (position < normalized.size()
               && (normalized[position] == ' ' || normalized[position] == '\t'))
            ++position;
        if (position == normalized.size())
            break;
        if (normalized[position] != ',')
        {
            error = "expected comma between turn numbers";
            return false;
        }
        ++position;
        if (position == normalized.size())
        {
            error = "trailing comma in turn-number list";
            return false;
        }
    }

    indices = std::move(decoded);
    return true;
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
    for (std::size_t i = 0; i < history.size(); ++i)
    {
        history_text += "[" + std::to_string(i + 1) + "]\n"
            "USER: " + history[i].m_user_text
            + "\nASSISTANT: " + history[i].m_assistant_text + "\n\n";
    }

    const std::string prompt =
        "Select the prior conversation turns that may help answer the current user request. "
        "Each numbered item is a complete pair: the user's question and the agent's answer. "
        "Return only the 1-based numbers of relevant pairs, separated by commas, for example: 1, 3. "
        "If no pair is relevant, return exactly NONE. Do not explain. "
        "Never invent a number or return a number outside the supplied list.";
    const std::string input =
        "CURRENT USER REQUEST:\n" + current_request
        + "\n\nNUMBERED PRIOR QUESTION-ANSWER PAIRS:\n" + history_text;

    const auto selection = m_llm.request<std::vector<std::size_t>>(
        ELlmRole::ThinkerLocal, prompt, input,
        [count = history.size()](const std::string& text,
                                 std::vector<std::size_t>& value,
                                 std::string& error)
        {
            return decode_relevant_indices(text, count, value, error);
        });

    result.m_ok = selection.m_ok;
    result.m_escalated = selection.m_escalated;
    result.m_used_role = selection.m_used_role;
    result.m_error = selection.m_error;
    result.m_relevant_turn_indices = selection.m_ok
        ? selection.m_value : std::vector<std::size_t>{};
    result.m_history_needed = selection.m_ok && !selection.m_value.empty();
    return result;
}
} // namespace fcpp
