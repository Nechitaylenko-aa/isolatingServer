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
    "  <scope>CODEBASE|GENERAL_ENGINEERING</scope>\n"
    "  <area>UNDERSTAND|DIAGNOSE|DESIGN|CHANGE|VERIFY|REVIEW|ENV</area>\n"
    "  <relation>SELF|CALLERS|CALLEES|OVERRIDES|REFERENCES|INCLUDES</relation>\n"
    "  <projection>SIGNATURE|SOURCE|EXPLANATION|LOCATIONS|GRAPH</projection>\n"
    "  <subject kind=\"CLASS|METHOD|FUNCTION|FILE|SYMBOL|...\" name=\"qualified_name\"/>\n"
    "</intent>\n"
    "Rules for <scope>:\n"
    "- GENERAL_ENGINEERING: conceptual C++/software engineering questions and new component design not asking for facts from the current repository. No project, file, cursor, or symbol is required.\n"
    "- CODEBASE: questions about existing code, project-specific architecture, diagnostics, changes, builds, and tests.\n"
    "- If repository-specific constraints matter, choose CODEBASE. Missing cursor/file context alone never means GENERAL_ENGINEERING.\n"
    "Rules for <area>:\n"
    "- Questions about the STATE of existing code are UNDERSTAND (not VERIFY). "
    "VERIFY is ONLY for building/running tests; DESIGN is ONLY for proposing new design.\n"
    "Rules for <relation>:\n"
    "- 'реализован/переопределён/override/заглушка/implemented/overridden/stub' -> OVERRIDES "
    "(NOT REFERENCES). REFERENCES is for plain usages.\n"
    "Rules for <subject>:\n"
    "- kind is the grammatical category the user names (e.g. 'класс' -> CLASS, 'метод' -> METHOD).\n"
    "- name must contain ONLY a symbol actually named in the request.\n"
    "- If a '[cursor symbol: NAME (KIND)]' hint is present, treat NAME as the subject the "
    "user points at: if the question is about it, set name=NAME and selection=CURSOR. "
    "If the question is not about it (global/project question), ignore the hint.\n"
    "- If the request is deictic ('этот класс', 'эта функция', 'this', 'here') and no symbol "
    "is named explicitly, set name=\"\" (empty). NEVER guess a name from the file name or path.\n";

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

/**
 * @brief Привести строку к нижнему регистру (ASCII + кириллица не требуется — ключи латиницей/кириллицей).
 * @param text Текст.
 * @return Строка в нижнем регистре.
 */
std::string to_lower(std::string text)
{
    for (char& symbol : text)
    {
        if (symbol >= 'A' && symbol <= 'Z')
        {
            symbol = static_cast<char>(symbol - 'A' + 'a');
        }
    }
    return text;
}

/**
 * @brief Содержит ли текст одну из подстрок.
 * @param haystack Текст (нижний регистр).
 * @param needles Подстроки.
 * @return true если найдена хотя бы одна.
 */
bool contains_any(const std::string& haystack, const std::vector<std::string>& needles)
{
    for (const std::string& needle : needles)
    {
        if (haystack.find(needle) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Извлечь имя символа из машинного cursor hint.
 *
 * Ожидаемый формат: [cursor symbol: qualified::Name (KIND)].
 */
std::string cursor_symbol_name(const std::string& cursor_hint, ESubjectKind& kind)
{
    constexpr const char* k_prefix = "[cursor symbol: ";
    const std::size_t start = cursor_hint.find(k_prefix);
    if (start == std::string::npos)
    {
        return {};
    }
    const std::size_t name_start = start + std::char_traits<char>::length(k_prefix);
    const std::size_t end = cursor_hint.find(']', name_start);
    if (end == std::string::npos)
    {
        return {};
    }
    const std::string value = cursor_hint.substr(name_start, end - name_start);
    const std::size_t kind_start = value.rfind(" (");
    if (kind_start == std::string::npos || value.back() != ')')
    {
        return {};
    }

    const std::string name = trim(value.substr(0, kind_start));
    const std::string kind_text = value.substr(kind_start + 2, value.size() - kind_start - 3);
    if (name.empty())
    {
        return {};
    }
    parse_subject_kind(kind_text, kind);
    return name;
}

/** @brief Проверить, ссылается ли запрос на объект под курсором. */
bool is_deictic_reference(const std::string& user_text)
{
    // Intent sees the assembled conversation context. Restrict the check to the
    // current user message so an old deictic phrase cannot capture today's cursor.
    constexpr const char* k_current_message = "[Current user message]";
    const std::size_t current_start = user_text.rfind(k_current_message);
    const std::string current_text = current_start == std::string::npos
        ? user_text
        : user_text.substr(current_start + std::char_traits<char>::length(k_current_message));
    const std::string text = to_lower(current_text);
    return contains_any(text, {
        "этот класс", "эта класс", "этого класса", "в этом классе",
        "эта функция", "эту функцию", "этой функции",
        "этот метод", "этого метода", "этим методом",
        "этот символ", "этой сущности", "здесь", "тут",
        "this class", "this function", "this method", "this symbol",
        "here is", "here in", "look here", "at the cursor"
    });
}

/**
 * @brief Детерминированная пост-коррекция Intent по ключевым словам (Секция 3, ADR-18 №1).
 * @param user_text Текст запроса.
 * @param intent Корректируемый Intent.
 */
void apply_keyword_correction(const std::string& user_text, SIntent& intent)
{
    const std::string text = to_lower(user_text);

    // "реализован/переопределён/заглушка/override/stub" -> OVERRIDES (§23).
    if (contains_any(text, {"реализов", "переопредел", "override", "заглушк",
                            "overriden", "overridden", "stub", "not implemented"}))
    {
        intent.m_relation = ERelation::Overrides;
    }

    // Вопрос о состоянии кода -> UNDERSTAND, а не DESIGN (§19).
    if (intent.m_area == EArea::Design
            && contains_any(text, {"реализов", "переопредел", "заглушк", "override"}))
    {
        intent.m_area = EArea::Understand;
    }
}

} // namespace

bool CIntentAnalyzer::parse_intent_xml(const std::string& xml, SIntent& out_intent, std::string& out_error)
{
    SIntent intent;

    const std::string scope_text = to_lower(trim(extract_tag(xml, "scope")));
    if (scope_text == "general_engineering")
    {
        intent.m_scope = ERequestScope::GeneralEngineering;
    }
    else if (scope_text.empty() || scope_text == "codebase")
    {
        intent.m_scope = ERequestScope::Codebase;
    }
    else
    {
        out_error = "cannot parse scope";
        return false;
    }

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
    if (!kind_text.empty() && !parse_subject_kind(kind_text, kind))
    {
        out_error = "cannot parse subject kind";
        return false;
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
                              const std::string& cursor_hint,
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

    if (!parse_intent_xml(reply.m_text, out_intent, out_error))
    {
        return false;
    }

    apply_keyword_correction(user_text, out_intent);

    // Если пользователь явно указывает на объект под курсором, не полагаемся
    // исключительно на то, вспомнит ли классификатор заполнить subject.name.
    // Для глобальных вопросов cursor hint намеренно не применяется.
    if (out_intent.m_subject.m_name.empty()
            && out_intent.m_subject.m_selection == ESubjectSelection::Context
            && is_deictic_reference(user_text))
    {
        ESubjectKind cursor_kind = out_intent.m_subject.m_kind;
        const std::string cursor_name = cursor_symbol_name(cursor_hint, cursor_kind);
        if (!cursor_name.empty())
        {
            out_intent.m_subject.m_name = cursor_name;
            out_intent.m_subject.m_kind = cursor_kind;
            out_intent.m_subject.m_selection = ESubjectSelection::Cursor;
        }
    }
    return true;
}

} // namespace fcpp
