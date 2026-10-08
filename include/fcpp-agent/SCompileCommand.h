/**
 * @file SCompileCommand.h
 * @brief Запись компиляции translation unit (ADR-04).
 *
 * Соответствует элементу массива compile_commands.json.
 */

#ifndef FCPP_AGENT_SCOMPILECOMMAND_H
#define FCPP_AGENT_SCOMPILECOMMAND_H

#include <filesystem>
#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Одна команда компиляции translation unit.
 */
struct SCompileCommand
{
    std::filesystem::path m_file;        ///< Абсолютный путь к компилируемому файлу.
    std::filesystem::path m_directory;   ///< Рабочая директория компиляции.
    std::vector<std::string> m_arguments;///< Аргументы компилятора (если заданы).
    std::string m_command;               ///< Командная строка (если задана).
    std::string m_language;              ///< Язык (c++/c), если указан.
};

} // namespace fcpp

#endif // FCPP_AGENT_SCOMPILECOMMAND_H
