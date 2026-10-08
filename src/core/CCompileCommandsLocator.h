/**
 * @file CCompileCommandsLocator.h
 * @brief Детерминированный локатор compile_commands.json (ADR-04).
 *
 * Zero-Search: файл ищется строго в трёх источниках по приоритету и никогда не
 * ищется рекурсивно по дереву проекта. Отсутствие файла во всех источниках —
 * это жёсткий FAIL, а не повод угадывать или генерировать флаги.
 */

#ifndef FCPP_AGENT_CCOMPILECOMMANDSLOCATOR_H
#define FCPP_AGENT_CCOMPILECOMMANDSLOCATOR_H

#include <filesystem>

namespace fcpp
{

/**
 * @brief Результат поиска файла compile_commands.json.
 */
struct SCompileCommandsLocation
{
    bool m_found;               ///< Найден ли файл.
    std::filesystem::path m_path;///< Путь к файлу (если найден).
};

/**
 * @brief Локатор compile_commands.json по трём разрешённым источникам.
 */
class CCompileCommandsLocator
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CCompileCommandsLocator() = delete;

    /**
     * @brief Найти файл по приоритету источников.
     *
     * Приоритет: 1) явный путь (CLI), 2) путь из конфига проекта,
     * 3) <project_root>/<build_dir>/compile_commands.json. Первый существующий
     * регулярный файл выигрывает. Рекурсивный поиск не выполняется.
     * @param project_root Канонический корень CMake-проекта.
     * @param explicit_path Путь из CLI (может быть пустым).
     * @param config_path Путь из конфига проекта (может быть пустым).
     * @param build_dir Имя build-каталога (default_build из конфига).
     * @return Результат поиска (m_found=false, если файла нет нигде).
     */
    static SCompileCommandsLocation resolve(const std::filesystem::path& project_root,
                                            const std::filesystem::path& explicit_path,
                                            const std::filesystem::path& config_path,
                                            const std::string& build_dir);
};

} // namespace fcpp

#endif // FCPP_AGENT_CCOMPILECOMMANDSLOCATOR_H
