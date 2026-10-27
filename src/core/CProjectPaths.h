/**
 * @file CProjectPaths.h
 * @brief Единый контракт разрешения путей проекта.
 */
#ifndef FCPP_AGENT_CPROJECTPATHS_H
#define FCPP_AGENT_CPROJECTPATHS_H

#include <filesystem>

namespace fcpp
{

/**
 * @brief Единый резолвер путей в абсолютный лексически нормализованный формат.
 *
 * Абсолютный вход нормализуется без изменения базы. Относительный вход
 * разрешается только относительно явно переданной абсолютной базы.
 */
class CProjectPaths
{
public:
    static std::filesystem::path resolve(const std::filesystem::path& value,
                                         const std::filesystem::path& base);
};

} // namespace fcpp

#endif // FCPP_AGENT_CPROJECTPATHS_H
