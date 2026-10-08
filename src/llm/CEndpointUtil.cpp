/**
 * @file CEndpointUtil.cpp
 * @brief Реализация сборки URL (6b).
 */

#include "llm/CEndpointUtil.h"

#include <string>

namespace fcpp
{

std::string CEndpointUtil::join(const std::string& base_url, const std::string& path)
{
    std::string base = base_url;

    // Убираем завершающие слэши базы.
    while (!base.empty() && base.back() == '/')
    {
        base.pop_back();
    }

    // Убираем ведущие слэши пути.
    std::size_t start = 0;
    while (start < path.size() && path[start] == '/')
    {
        ++start;
    }

    if (base.empty())
    {
        return path;
    }
    return base + "/" + path.substr(start);
}

} // namespace fcpp
