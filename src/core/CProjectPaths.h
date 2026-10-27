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
 * @brief Разрешить путь в абсолютный лексически нормализованный путь.
 *
 * Абсолютный вход нормализуется без изменения базы. Относительный вход
 * разрешается только относительно явно переданной абсолютной базы: скрытая
 * зависимость от current_path() запрещена.
 */
class CProjectPaths
{
public:
    static std::filesystem::path resolve(const std::filesystem::path& value,
                                         const std::filesystem::path& base)
    {
        if (value.empty())
        {
            return {};
        }
        if (value.is_absolute())
        {
            return value.lexically_normal();
        }
        if (base.empty() || !base.is_absolute())
        {
            return {};
        }
        return (base / value).lexically_normal();
    }
};

} // namespace fcpp

#endif // FCPP_AGENT_CPROJECTPATHS_H
