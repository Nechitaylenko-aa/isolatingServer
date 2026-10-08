/**
 * @file SIntent.cpp
 * @brief Имена и парсеры Intent-таксономии (Секция 3 rules.md).
 */

#include "fcpp-agent/SIntent.h"

#include <cstddef>
#include <string>

namespace fcpp
{

namespace
{

/** @brief Имена областей, индексированные по EArea. */
static const char* const m_area_names[static_cast<std::size_t>(EArea::Count)]
{
    "UNDERSTAND", "DIAGNOSE", "DESIGN", "CHANGE", "VERIFY", "REVIEW", "ENV"
};

/** @brief Имена типов субъектов, индексированные по ESubjectKind. */
static const char* const m_subject_kind_names[static_cast<std::size_t>(ESubjectKind::Count)]
{
    "PROJECT", "FILE", "NAMESPACE", "CLASS", "STRUCT", "ENUM",
    "FUNCTION", "METHOD", "VARIABLE", "FIELD", "SYMBOL", "BUILD_TARGET"
};

/** @brief Имена отношений, индексированные по ERelation. */
static const char* const m_relation_names[static_cast<std::size_t>(ERelation::Count)]
{
    "SELF", "CALLERS", "CALLEES", "OVERRIDES", "REFERENCES", "INCLUDES"
};

/** @brief Имена проекций, индексированные по EProjection. */
static const char* const m_projection_names[static_cast<std::size_t>(EProjection::Count)]
{
    "SIGNATURE", "SOURCE", "EXPLANATION", "LOCATIONS", "GRAPH"
};

/** @brief Имена воздействий, индексированные по EEffect. */
static const char* const m_effect_names[static_cast<std::size_t>(EEffect::Count)]
{
    "READ_ONLY", "WRITE_SOURCE", "EXECUTE", "WRITE_BUILD"
};

/**
 * @brief Сопоставить текст с таблицей имён (без switch-case).
 * @param text Текст.
 * @param names Таблица имён.
 * @param count Размер таблицы.
 * @param out Выходной индекс.
 * @return true если найдено.
 */
bool match_name(const std::string& text, const char* const* names, std::size_t count, std::size_t& out)
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (text == names[index])
        {
            out = index;
            return true;
        }
    }
    return false;
}

} // namespace

const char* area_name(EArea area)
{
    return m_area_names[static_cast<std::size_t>(area)];
}

const char* subject_kind_name(ESubjectKind kind)
{
    return m_subject_kind_names[static_cast<std::size_t>(kind)];
}

const char* relation_name(ERelation relation)
{
    return m_relation_names[static_cast<std::size_t>(relation)];
}

const char* projection_name(EProjection projection)
{
    return m_projection_names[static_cast<std::size_t>(projection)];
}

const char* effect_name(EEffect effect)
{
    return m_effect_names[static_cast<std::size_t>(effect)];
}

bool parse_area(const std::string& text, EArea& out)
{
    std::size_t index = 0;
    if (!match_name(text, m_area_names, static_cast<std::size_t>(EArea::Count), index))
    {
        return false;
    }
    out = static_cast<EArea>(index);
    return true;
}

bool parse_relation(const std::string& text, ERelation& out)
{
    std::size_t index = 0;
    if (!match_name(text, m_relation_names, static_cast<std::size_t>(ERelation::Count), index))
    {
        return false;
    }
    out = static_cast<ERelation>(index);
    return true;
}

bool parse_projection(const std::string& text, EProjection& out)
{
    std::size_t index = 0;
    if (!match_name(text, m_projection_names, static_cast<std::size_t>(EProjection::Count), index))
    {
        return false;
    }
    out = static_cast<EProjection>(index);
    return true;
}

bool parse_subject_kind(const std::string& text, ESubjectKind& out)
{
    std::size_t index = 0;
    if (!match_name(text, m_subject_kind_names, static_cast<std::size_t>(ESubjectKind::Count), index))
    {
        return false;
    }
    out = static_cast<ESubjectKind>(index);
    return true;
}

} // namespace fcpp
