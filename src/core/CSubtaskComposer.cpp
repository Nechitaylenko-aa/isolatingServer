/**
 * @file CSubtaskComposer.cpp
 * @brief Реализация декомпозитора (Фаза A, ADR-17).
 */

#include "core/CSubtaskComposer.h"

#include <cstddef>
#include <string>
#include <vector>

namespace fcpp
{

namespace
{

/** @brief Системный промт декомпозитора (стабильный префикс для prompt_cache). */
constexpr const char* k_compose_prompt =
    "You decompose a complex C++ question into an ORDERED list of simpler, ATOMIC sub-questions.\n"
    "Return ONLY the sub-questions, ONE PER LINE, no numbering, no preamble, no explanation.\n"
    "Do NOT echo any bracketed hint lines; use them only as context.\n"
    "Rules:\n"
    "- Each line is a SINGLE atomic lookup answerable from a code index.\n"
    "- PRESERVE THE ANCHOR: keep the exact subject the user refers to; "
    "do NOT generalize it to the whole project. If a hierarchy anchor hint is given, use it.\n"
    "- For questions about DERIVED classes / overrides / 'all descendants', keep the "
    "HIERARCHY anchor: use the type the question is about (its base type), NOT a specific "
    "leaf descendant. Phrase sub-questions so the core can walk the inheritance graph.\n"
    "- If the request is already atomic, output exactly ONE line with the original request.\n"
    "- Keep the order: find the set, then check the predicate, then inspect method bodies.\n"
    "- Rely only on these capabilities: type outline, derived classes, overrides, references, "
    "callers/callees, includes, and source of a method.\n";

/**
 * @brief Убрать ведущий маркер списка и пробелы.
 * @param line Строка.
 * @return Очищенная строка.
 */
std::string strip_bullet(const std::string& line)
{
    std::size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        return {};
    }
    std::size_t end = line.find_last_not_of(" \t\r\n");

    // Ведущие маркеры: "-", "*", "1.", "1)", "N-".
    std::size_t cursor = start;
    if (cursor <= end && (line[cursor] == '-' || line[cursor] == '*'))
    {
        ++cursor;
    }
    else
    {
        std::size_t digit = cursor;
        while (digit <= end && line[digit] >= '0' && line[digit] <= '9')
        {
            ++digit;
        }
        if (digit > cursor && digit <= end
                && (line[digit] == '.' || line[digit] == ')' || line[digit] == '-'))
        {
            cursor = digit + 1;
        }
    }

    if (cursor > end)
    {
        return {};
    }
    const std::size_t content = line.find_first_not_of(" \t", cursor);
    if (content == std::string::npos || content > end)
    {
        return {};
    }
    return line.substr(content, end - content + 1);
}

} // namespace

bool CSubtaskComposer::parse_subtask_list(const std::string& text,
                                          std::size_t max_subtasks,
                                          SSubtaskPlan& out_plan,
                                          std::string& out_error)
{
    out_plan = SSubtaskPlan{};
    out_error.clear();

    std::size_t start = 0;
    while (start <= text.size() && out_plan.m_subtasks.size() < max_subtasks)
    {
        std::size_t end = text.find('\n', start);
        const std::string raw = (end == std::string::npos)
                                    ? text.substr(start)
                                    : text.substr(start, end - start);

        const std::string question = strip_bullet(raw);
        if (!question.empty())
        {
            SSubTask subtask;
            subtask.m_question = question;
            out_plan.m_subtasks.push_back(std::move(subtask));
        }

        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }

    if (out_plan.m_subtasks.empty())
    {
        out_error = "no subtasks in model output";
        return false;
    }

    out_plan.m_compound = out_plan.m_subtasks.size() > 1;
    return true;
}

bool CSubtaskComposer::compose(IModelApi& model,
                               const std::string& user_text,
                               const std::string& cursor_hint,
                               std::size_t max_subtasks,
                               SSubtaskPlan& out_plan,
                               std::string& out_error) const
{
    if (!model.is_available())
    {
        out_error = "lite model is not available";
        return false;
    }
    if (max_subtasks == 0)
    {
        out_error = "max_subtasks is zero";
        return false;
    }

    std::vector<SModelMessage> messages;
    messages.push_back(SModelMessage{EModelRole::System, k_compose_prompt});

    std::string user_content = user_text;
    if (!cursor_hint.empty())
    {
        user_content += "\n" + cursor_hint;
    }
    messages.push_back(SModelMessage{EModelRole::User, user_content});

    const SModelReply reply = model.complete(messages);
    if (!reply.m_ok)
    {
        out_error = reply.m_error.empty() ? "model request failed" : reply.m_error;
        return false;
    }

    return parse_subtask_list(reply.m_text, max_subtasks, out_plan, out_error);
}

} // namespace fcpp
