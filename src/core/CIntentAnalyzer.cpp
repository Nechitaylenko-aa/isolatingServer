/**
 * @file CIntentAnalyzer.cpp
 * @brief Реализация анализатора Intent (6b).
 */

#include "core/CIntentAnalyzer.h"

#include <cstddef>
#include <string>
#include <vector>

namespace fcpp
{

namespace
{

/** @brief Системный промт классификатора (стабильный префикс для prompt_cache). */
constexpr const char* k_system_prompt =
    "You are a C++/CMake request classifier. Return ONLY this XML, no prose:\n"
    "<intent>\n"
    "  <area>UNDERSTAND|DIAGNOSE|DESIGN|CHANGE|VERIFY|REVIEW|ENV</area>\n"
    "  <relation>SELF|CALLERS|CALLEES|OVERRIDES|REFERENCES|INCLUDES</relation>\n"
    "  <projection>SIGNATURE|SOURCE|EXPLANATION|LOCATIONS|GRAPH</projection>\n"
    "  <subject kind=\"CLASS|METHOD|FUNCTION|FILE|SYMBOL|...\" name=\"qualified_name\"/>\n"
    "</intent>\n";

/**
 * @brief Извлечь содержимое тега.
 * @param xml Текст.
 * @param tag Имя тега.
 * @return Содержимое либо пустая строка.
 */
std::string extract_tag(const std::string& xml, const std::string& tag)
{
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";

    const std::size_t start = xml.find(open);
    if (start == std::string::npos)
    {
        return {};
    }
    const std::size_t content_start = start + open.size();
    const std::size_t end = xml.find(close, content_start);
    if (end == std::string::npos)
    {
        return {};
    }
    return xml.substr(content_start, end - content_start);
}

/**
 * @brief Извлечь XML-атрибут.
 * @param xml Текст.
 * @param attribute Имя атрибута.
 * @return Значение либо пустая строка.
 */
std::string extract_attribute(const std::string& xml, const std::string& attribute)
{
    const std::string key = attribute + "=\"";
    const std::size_t start = xml.find(key);
    if (start == std::string::npos)
    {
        return {};
    }
    const std::size_t value_start = start + key.size();
    const std::size_t end = xml.find('"', value_start);
    if (end == std::string::npos)
    {
        return {};
    }
    return xml.substr(value_start, end - value_start);
}

/**
 * @brief Обрезать пробелы по краям.
 * @param text Текст.
 * @return Строка без крайних пробелов.
 */
std::string trim(const std::string& text)
{
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

} // namespace

bool CIntentAnalyzer::parse_intent_xml(const std::string& xml, SIntent& out_intent, std::string& out_error)
{
    SIntent intent;

    if (!parse_area(trim(extract_tag(xml, "area")), intent.m_area))
    {
        out_error = "cannot parse area";
        return false;
    }
    if (!parse_relation(trim(extract_tag(xml, "relation")), intent.m_relation))
    {
        out_error = "cannot parse relation";
        return false;
    }
    if (!parse_projection(trim(extract_tag(xml, "projection")), intent.m_projection))
    {
        out_error = "cannot parse projection";
        return false;
    }

    const std::size_t subject_start = xml.find("<subject");
    std::string subject_fragment;
    if (subject_start != std::string::npos)
    {
        const std::size_t subject_end = xml.find('>', subject_start);
        subject_fragment = xml.substr(subject_start, subject_end - subject_start + 1);
    }

    const std::string kind_text = trim(extract_attribute(subject_fragment, "kind"));
    ESubjectKind kind = ESubjectKind::Symbol;
    if (!kind_text.empty())
    {
        parse_subject_kind(kind_text, kind);
    }
    intent.m_subject.m_kind = kind;
    intent.m_subject.m_name = extract_attribute(subject_fragment, "name");
    intent.m_subject.m_selection = intent.m_subject.m_name.empty()
                                       ? ESubjectSelection::Context
                                       : ESubjectSelection::ExplicitName;

    // Effect выводится детерминированно (§26), не моделью.
    intent.m_effect = (intent.m_area == EArea::Change) ? EEffect::WriteSource : EEffect::ReadOnly;
    intent.m_confidence = 80;

    out_intent = intent;
    return true;
}

bool CIntentAnalyzer::analyze(IModelApi& model,
                              const std::string& user_text,
                              const std::string& cursor_file,
                              unsigned cursor_line,
                              SIntent& out_intent,
                              std::string& out_error) const
{
    if (!model.is_available())
    {
        out_error = "lite model is not available";
        return false;
    }

    std::vector<SModelMessage> messages;
    messages.push_back(SModelMessage{EModelRole::System, k_system_prompt});

    std::string user_content = user_text;
    if (!cursor_file.empty() && cursor_line > 0)
    {
        user_content += "\n[cursor: " + cursor_file + ":" + std::to_string(cursor_line) + "]";
    }
    messages.push_back(SModelMessage{EModelRole::User, user_content});

    const SModelReply reply = model.complete(messages);
    if (!reply.m_ok)
    {
        out_error = reply.m_error.empty() ? "model request failed" : reply.m_error;
        return false;
    }

    return parse_intent_xml(reply.m_text, out_intent, out_error);
}

} // namespace fcpp
