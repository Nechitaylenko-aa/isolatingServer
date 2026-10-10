/**
 * @file SEvidence.h
 * @brief Модель Evidence для подачи в LLM (Zero-Window, §56).
 *
 * Evidence — минимальный набор фактов, собранных из индекса. Файлы целиком не
 * передаются: только сигнатуры, тела затребованных методов и relationships.
 */

#ifndef FCPP_AGENT_SEVIDENCE_H
#define FCPP_AGENT_SEVIDENCE_H

#include <string>
#include <vector>

#include "fcpp-agent/ITool.h"

namespace fcpp
{

/**
 * @brief Секция Evidence (результат одного инструмента).
 */
struct SEvidenceSection
{
    EToolName m_tool = EToolName::Outline; ///< Источник секции.
    std::string m_title;                   ///< Заголовок секции.
    std::string m_text;                    ///< Текст (минимальный срез).
};

/**
 * @brief Набор Evidence.
 */
struct SEvidence
{
    std::vector<SEvidenceSection> m_sections; ///< Секции.

    /**
     * @brief Есть ли содержимое.
     * @return true если хотя бы одна секция непуста.
     */
    bool empty() const
    {
        return m_sections.empty();
    }
};

/**
 * @brief Собрать Evidence в компактный текст для промта.
 * @param evidence Набор Evidence.
 * @return Текст (пустой, если секций нет).
 */
std::string format_evidence(const SEvidence& evidence);

} // namespace fcpp

#endif // FCPP_AGENT_SEVIDENCE_H
