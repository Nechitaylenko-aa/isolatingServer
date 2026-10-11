/**
 * @file CCompileCommandsParser.h
 * @brief Парсер compile_commands.json (ADR-04).
 *
 * Разбирает JSON-массив записей компиляции в типизированные SCompileCommand.
 * Невалидный JSON или отсутствие обязательных полей — FAIL.
 */

#ifndef FCPP_AGENT_CCOMPILECOMMANDSPARSER_H
#define FCPP_AGENT_CCOMPILECOMMANDSPARSER_H

#include <filesystem>
#include <string>
#include <vector>

#include "fcpp-agent/SCompileCommand.h"

namespace fcpp
{

/**
 * @brief Парсер файла compile_commands.json.
 */
class CCompileCommandsParser
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CCompileCommandsParser() = delete;

    /**
     * @brief Разобрать файл compile_commands.json.
     *
     * Каждая запись обязана содержать абсолютную "directory", непустой "file"
     * и хотя бы одно из "command"/"arguments". Относительный "file" разрешается
     * относительно directory.
     * @param json_path Путь к файлу compile_commands.json.
     * @param out_commands Выходной список команд.
     * @return true при успехе; false при ошибке чтения/парсинга/валидации.
     */
    static bool parse(const std::filesystem::path& json_path,
                      std::vector<SCompileCommand>& out_commands);
};

} // namespace fcpp

#endif // FCPP_AGENT_CCOMPILECOMMANDSPARSER_H
