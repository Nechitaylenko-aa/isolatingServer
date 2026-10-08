/**
 * @file SPatch.h
 * @brief Модель патча (unified diff) для контролируемого изменения кода (§44).
 */

#ifndef FCPP_AGENT_SPATCH_H
#define FCPP_AGENT_SPATCH_H

#include <cstdint>
#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Строка hunk'а патча.
 */
struct SPatchLine
{
    char m_type = ' ';       ///< ' ' (контекст), '-' (удаление), '+' (добавление).
    std::uint32_t m_line = 0;///< Номер строки в исходном/новом файле.
    std::string m_text;      ///< Содержимое строки (без завершающего перевода).
};

/**
 * @brief Hunk (непрерывный блок изменений).
 */
struct SPatchHunk
{
    std::uint32_t m_old_start = 0; ///< Начальная строка в исходном файле.
    std::uint32_t m_old_count = 0; ///< Число строк исходного файла.
    std::uint32_t m_new_start = 0; ///< Начальная строка в новом файле.
    std::uint32_t m_new_count = 0; ///< Число строк нового файла.
    std::vector<SPatchLine> m_lines;///< Строки hunk'а.
};

/**
 * @brief Патч для одного файла.
 */
struct SFilePatch
{
    std::string m_old_path;         ///< Путь в исходном файле (без префиксов a/ b/).
    std::string m_new_path;         ///< Путь в новом файле.
    bool m_is_create = false;       ///< Новый файл (--- /dev/null).
    bool m_is_delete = false;       ///< Удаление файла (+++ /dev/null).
    std::vector<SPatchHunk> m_hunks;///< Блоки изменений.
};

/**
 * @brief Разобранный набор патчей.
 */
struct SPatchSet
{
    std::vector<SFilePatch> m_files;///< Файловые патчи.
};

/**
 * @brief Запланированное изменение одного файла (результат валидации).
 */
struct SFileChange
{
    std::string m_path;             ///< Абсолютный путь к файлу.
    bool m_is_create = false;       ///< Создание нового файла.
    bool m_is_delete = false;       ///< Удаление файла.
    bool m_existed = false;         ///< Существовал ли файл до изменения.
    std::string m_original;         ///< Исходное содержимое (для отката).
    std::string m_updated;          ///< Новое содержимое.
};

/**
 * @brief Результат валидации патча.
 */
struct SPatchValidation
{
    bool m_valid = false;                    ///< Патч корректен и применим целиком.
    std::string m_message;                   ///< Сообщение (при ошибке).
    std::vector<SFileChange> m_changes;      ///< Запланированные изменения.
};

} // namespace fcpp

#endif // FCPP_AGENT_SPATCH_H
