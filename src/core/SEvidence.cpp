/**
 * @file SEvidence.cpp
 * @brief Реализация форматирования Evidence (Zero-Window).
 */

#include "fcpp-agent/SEvidence.h"

#include <string>

namespace fcpp
{

std::string format_evidence(const SEvidence& evidence)
{
    std::string text;
    for (const SEvidenceSection& section : evidence.m_sections)
    {
        if (section.m_text.empty())
        {
            continue;
        }
        text += "[";
        text += section.m_title;
        text += "]\n";
        text += section.m_text;
        if (!text.empty() && text.back() != '\n')
        {
            text += '\n';
        }
        text += '\n';
    }
    return text;
}

} // namespace fcpp
