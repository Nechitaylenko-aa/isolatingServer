/**
 * @file CProjectProfile.h
 * @brief Минимальный паспорт проекта.
 */

#ifndef FCPP_AGENT_CPROJECTPROFILE_H
#define FCPP_AGENT_CPROJECTPROFILE_H

#include <string>

#include "fcpp-agent/IModelApi.h"

namespace fcpp
{

/**
 * @brief Исходное описание и рабочий текст паспорта проекта.
 *
 * zip() пока является намеренной заглушкой: вызов не меняет содержимое.
 */
class CProjectProfile
{
public:
    void set(std::string original_text);
    /** @brief Restore persisted original and compact text without recompression. */
    void restore(std::string original_text, std::string compact_text);
    const std::string& original() const;
    const std::string& get() const;
    void zip(IModelApi& model);

private:
    std::string m_original_text;
    std::string m_compact_text;
};

} // namespace fcpp

#endif // FCPP_AGENT_CPROJECTPROFILE_H
