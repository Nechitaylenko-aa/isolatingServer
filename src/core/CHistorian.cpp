#include "core/CHistorian.h"

#include <algorithm>
#include <charconv>
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

bool decode_selection(const std::string& text, std::vector<std::size_t>& value,
                      std::string& error, std::size_t history_size)
{
    const std::string answer = normalize_answer(text);
    if (answer == "NONE")
    {
        value.clear();
        return true;
    }

    if (answer.empty())
    {
        error = "expected NONE or comma-separated turn numbers";
        return false;
    }

    std::vector<std::size_t> selected;
    std::size_t start = 0;
    while (start < answer.size())
    {
        const std::size_t comma = answer.find(',', start);
        const std::size_t end = comma == std::string::npos ? answer.size() : comma;
        const std::string token = answer.substr(start, end - start);
        const std::string number = normalize_answer(token);
        if (number.empty()
            || !std::all_of(number.begin(), number.end(),
                [](unsigned char ch) { return ch >= '0' && ch <= '9'; }))
        {
            error = "expected NONE or comma-separated positive turn numbers";
            return false;
        }

        std::size_t one_based = 0;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), one_based);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()
            || one_based == 0 || one_based > history_size)
        {
            error = "selected turn number is outside the supplied history";
            return false;
        }

        const std::size_t zero_based = one_based - 1;
        if (std::find(selected.begin(), selected.end(), zero_based) != selected.end())
        {
            error = "duplicate selected turn number";
            return false;
        }
        selected.push_back(zero_based);

        if (comma == std::string::npos)
            break;
        start = comma + 1;
        if (start == answer.size())
        {
            error = "trailing comma in selected turn numbers";
            return false;
        }
    }

    std::sort(selected.begin(), selected.end());
    value = std::move(selected);
    return true;
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
    for (std::size_t index = 0; index < history.size(); ++index)
    {
        const SConversationTurn& turn = history[index];
        history_text += "[" + std::to_string(index + 1) + "]\nUSER: "
            + turn.m_user_text + "\nASSISTANT: " + turn.m_assistant_text + "\n\n";
    }

    const std::string prompt =
        "Select the previous conversation turn pairs that may be relevant to answering "
        "the current user request. Each turn pair has a number and contains both the "
        "user question and the agent answer. Return only the selected turn numbers as "
        "a comma-separated list, using the numbers exactly as shown, in ascending order. "
        "If no turn is relevant, return exactly NONE. Do not explain, add punctuation, "
        "or return any text other than the numbers or NONE.";
    const std::string input =
        "CURRENT REQUEST:\n" + current_request
        + "\n\nNUMBERED PRIOR TURNS:\n" + history_text;

    const auto decision = m_llm.request<std::vector<std::size_t>>(
        ELlmRole::ThinkerLocal, prompt, input,
        [history_size = history.size()](const std::string& text,
                                        std::vector<std::size_t>& selected,
                                        std::string& error)
        {
            return decode_selection(text, selected, error, history_size);
        });

    result.m_ok = decision.m_ok;
    result.m_selected_turn_indices = decision.m_ok
        ? decision.m_value : std::vector<std::size_t>{};
    result.m_history_needed = result.m_ok && !result.m_selected_turn_indices.empty();
    result.m_escalated = decision.m_escalated;
    result.m_used_role = decision.m_used_role;
    result.m_error = decision.m_error;
    return result;
}
} // namespace fcpp
