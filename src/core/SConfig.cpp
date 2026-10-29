/**
 * @file SConfig.cpp
 * @brief Реализация имён типов провайдеров моделей (Секция 3 rules.md).
 */

#include "fcpp-agent/SConfig.h"

#include <cstddef>
#include <string>

namespace fcpp
{

namespace
{

/** @brief Имена типов remote-провайдеров, индексированные по EModelRemoteType. */
static const char* const m_remote_type_names[static_cast<std::size_t>(EModelRemoteType::Count)]
{
    "openai-compat",
    "claude"
};

} // namespace

const char* remote_type_name(EModelRemoteType type)
{
    return m_remote_type_names[static_cast<std::size_t>(type)];
}

EModelRemoteType remote_type_from_name(const std::string& name)
{
    for (std::size_t index = 0; index < static_cast<std::size_t>(EModelRemoteType::Count); ++index)
    {
        if (name == m_remote_type_names[index])
        {
            return static_cast<EModelRemoteType>(index);
        }
    }
    return EModelRemoteType::OpenAICompat;
}

} // namespace fcpp

