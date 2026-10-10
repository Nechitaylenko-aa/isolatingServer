#include "fcpp-agent/CProjectProfile.h"

#include <utility>

namespace fcpp
{

void CProjectProfile::set(std::string original_text)
{
    m_original_text = std::move(original_text);
    m_compact_text = m_original_text;
}

void CProjectProfile::restore(std::string original_text, std::string compact_text)
{
    m_original_text = std::move(original_text);
    m_compact_text = std::move(compact_text);
}

const std::string& CProjectProfile::original() const
{
    return m_original_text;
}

const std::string& CProjectProfile::get() const
{
    return m_compact_text;
}

void CProjectProfile::zip(IModelApi& model)
{
    // Deliberate no-op for the first integration pass.
    (void)model;
}

} // namespace fcpp
