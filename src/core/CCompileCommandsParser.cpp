/**
 * @file CCompileCommandsParser.cpp
 * @brief Реализация парсера compile_commands.json (ADR-04).
 */

#include "core/CCompileCommandsParser.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace fcpp
{

namespace
{

/** @brief Ключ записи: рабочая директория. */
constexpr const char* k_key_directory = "directory";

/** @brief Ключ записи: файл. */
constexpr const char* k_key_file = "file";

/** @brief Ключ записи: командная строка. */
constexpr const char* k_key_command = "command";

/** @brief Ключ записи: массив аргументов. */
constexpr const char* k_key_arguments = "arguments";

/** @brief Ключ записи: язык. */
constexpr const char* k_key_language = "language";

/**
 * @brief Развернуть путь относительно базовой директории.
 * @param base Базовая директория.
 * @param value Путь (абсолютный или относительный).
 * @return Нормализованный абсолютный путь.
 */
std::filesystem::path resolve_path(const std::filesystem::path& base, const std::string& value)
{
    const std::filesystem::path path(value);
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }
    return (base / path).lexically_normal();
}

/**
 * @brief Разбить командную строку на аргументы (учёт кавычек).
 * @param text Командная строка.
 * @return Вектор аргументов.
 */
std::vector<std::string> split_arguments(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string current;
    bool in_single = false;
    bool in_double = false;
    bool has_token = false;

    for (const char character : text)
    {
        if (character == '\'' && !in_double)
        {
            in_single = !in_single;
            has_token = true;
        }
        else if (character == '"' && !in_single)
        {
            in_double = !in_double;
            has_token = true;
        }
        else if ((character == ' ' || character == '\t') && !in_single && !in_double)
        {
            if (has_token)
            {
                tokens.push_back(current);
                current.clear();
                has_token = false;
            }
        }
        else
        {
            current.push_back(character);
            has_token = true;
        }
    }

    if (has_token)
    {
        tokens.push_back(current);
    }

    return tokens;
}

} // namespace

bool CCompileCommandsParser::parse(const std::filesystem::path& json_path,
                                   std::vector<SCompileCommand>& out_commands)
{
    out_commands.clear();

    std::ifstream input(json_path);
    if (!input.is_open())
    {
        return false;
    }

    nlohmann::json document;
    try
    {
        input >> document;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }

    if (!document.is_array())
    {
        return false;
    }

    std::vector<SCompileCommand> parsed;
    parsed.reserve(document.size());

    for (const nlohmann::json& entry : document)
    {
        if (!entry.is_object())
        {
            return false;
        }
        if (!entry.contains(k_key_directory) || !entry[k_key_directory].is_string())
        {
            return false;
        }
        if (!entry.contains(k_key_file) || !entry[k_key_file].is_string())
        {
            return false;
        }

        const std::filesystem::path directory(entry[k_key_directory].get<std::string>());
        const std::string file_value = entry[k_key_file].get<std::string>();

        const bool has_command = entry.contains(k_key_command) && entry[k_key_command].is_string();
        const bool has_arguments = entry.contains(k_key_arguments) && entry[k_key_arguments].is_array();
        if (!has_command && !has_arguments)
        {
            return false;
        }

        SCompileCommand command;
        command.m_directory = directory;
        command.m_file = resolve_path(directory, file_value);

        if (has_command)
        {
            command.m_command = entry[k_key_command].get<std::string>();
        }
        if (has_arguments)
        {
            for (const nlohmann::json& argument : entry[k_key_arguments])
            {
                if (!argument.is_string())
                {
                    return false;
                }
                command.m_arguments.push_back(argument.get<std::string>());
            }
        }
        if (command.m_arguments.empty() && !command.m_command.empty())
        {
            command.m_arguments = split_arguments(command.m_command);
        }
        if (entry.contains(k_key_language) && entry[k_key_language].is_string())
        {
            command.m_language = entry[k_key_language].get<std::string>();
        }

        parsed.push_back(std::move(command));
    }

    out_commands = std::move(parsed);
    return true;
}

} // namespace fcpp
