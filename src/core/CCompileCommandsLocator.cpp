/**
 * @file CCompileCommandsLocator.cpp
 * @brief Реализация локатора compile_commands.json (ADR-04).
 */

#include "core/CCompileCommandsLocator.h"

#include <filesystem>
#include <system_error>
#include <vector>

namespace fcpp
{

namespace
{

/** @brief Имя файла компиляционных команд. */
constexpr const char* k_compile_commands_name = "compile_commands.json";

/** @brief Стандартная поддиректория сборки. */
constexpr const char* k_build_dir_name = "build";

/**
 * @brief Проверить, что путь указывает на существующий регулярный файл.
 * @param path Проверяемый путь.
 * @return true если это существующий регулярный файл.
 */
bool is_existing_file(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return false;
    }

    std::error_code error_code;
    if (!std::filesystem::exists(path, error_code) || error_code)
    {
        return false;
    }

    return std::filesystem::is_regular_file(path, error_code) && !error_code;
}

} // namespace

SCompileCommandsLocation CCompileCommandsLocator::resolve(const std::filesystem::path& project_root,
                                                          const std::filesystem::path& explicit_path,
                                                          const std::filesystem::path& config_path,
                                                          const std::string& build_dir)
{
    const std::string effective_build = build_dir.empty() ? k_build_dir_name : build_dir;

    const std::vector<std::filesystem::path> candidates =
    {
        explicit_path,
        config_path,
        project_root / effective_build / k_compile_commands_name
    };

    for (const std::filesystem::path& candidate : candidates)
    {
        if (is_existing_file(candidate))
        {
            return SCompileCommandsLocation{true, candidate};
        }
    }

    return SCompileCommandsLocation{false, {}};
}

} // namespace fcpp
