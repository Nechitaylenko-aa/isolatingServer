#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cpptool
{

/**
 * @brief FNV-1a 64-bit хэш строки.
 *
 * Свой алгоритм вместо std::hash намеренно: std::hash не гарантирует
 * стабильность между версиями стандартной библиотеки, а хэши лежат в кэше
 * на диске и должны переживать апгрейд тулчейна, иначе кэш тихо теряет
 * все попадания.
 *
 * @param data входные байты
 * @return 64-битный хэш
 */
uint64_t fnv1a_64(const std::string& data);

/**
 * @brief FNV-1a 64-bit хэш содержимого файла.
 *
 * Если файл не читается, возвращается пустая строка — вызывающий код обязан
 * трактовать это как «содержимое неизвестно» (грязно), а не как валидный хэш.
 *
 * @param path абсолютный путь к файлу
 * @return hex-строка из 16 символов, либо пустая строка при ошибке чтения
 */
std::string hash_file(const std::string& path);

/**
 * @brief FNV-1a 64-bit хэш списка флагов компиляции.
 *
 * Флаги разделяются нулевым байтом, поэтому ["-a","b"] и ["-ab"] дают разные
 * хэши — без разделителя это была бы коллизия.
 *
 * @param flags список флагов
 * @return hex-строка из 16 символов
 */
std::string hash_flags(const std::vector<std::string>& flags);

/**
 * @brief FNV-1a 64-bit хэш строки, возвращённый как hex.
 *
 * @param data входная строка
 * @return hex-строка из 16 символов
 */
std::string hash_string(const std::string& data);

} // namespace cpptool
