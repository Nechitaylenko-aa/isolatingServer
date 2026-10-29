/**
 * @file IFactProvider.h
 * @brief Контракт поставщика фактов о кодовой базе для оркестратора.
 *
 * Оркестратор сам решает, какие факты запросить, и не знает, откуда они
 * берутся (SQLite-индекс, libclang, файлы). Поставщик не выбирает маршрут и
 * не вызывает модель.
 */

#ifndef FCPP_AGENT_IFACTPROVIDER_H
#define FCPP_AGENT_IFACTPROVIDER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Запрос фактов по сущности, известной оркестратору.
 */
struct SFactRequest
{
    std::string m_qualified_name; ///< Квалифицированное имя (приоритетный ключ).
    std::string m_name;           ///< Короткое имя.
    std::string m_file;           ///< Файл сущности (если известен).
    std::uint32_t m_line = 0;     ///< Строка сущности (0 — неизвестна).

    /**
     * @brief Файл, в котором сущность обязана быть объявлена.
     *
     * Имя сущности может прийти от модели, а не от libclang. Тогда это текст,
     * а не факт: модель может назвать класс из соседнего файла. Если поле
     * задано, поставщик подтверждает принадлежность по индексу и при
     * несовпадении возвращает пустой результат с `m_rejected = true`.
     */
    std::string m_expected_file;
};

/**
 * @brief Одна секция фактов, готовая к помещению в промпт.
 */
struct SFactSection
{
    std::string m_title; ///< Заголовок секции.
    std::string m_text;  ///< Содержимое (Zero-Window: только срез).
};

/**
 * @brief Результат сбора фактов.
 *
 * Отсутствие фактов — это `m_ok = true` с пустым списком секций. Технический
 * сбой — `m_ok = false` с причиной: оркестратор обязан различать эти случаи.
 */
struct SFactResult
{
    bool m_ok = true;                    ///< Сбор завершился без технического сбоя.
    bool m_rejected = false;             ///< Сущность не объявлена в ожидаемом файле.
    std::string m_error;                 ///< Причина сбоя.
    std::vector<SFactSection> m_sections;///< Собранные секции.
};

/**
 * @brief Поставщик фактов о кодовой базе.
 */
/**
 * @brief Ограничения ограниченного среза файла.
 *
 * Срез читается с начала файла и всегда укладывается в лимит: в модель не
 * уходит файл целиком (Zero-Window).
 */
struct SFileSliceLimits
{
    std::size_t m_max_bytes = 8000; ///< Жёсткий лимит размера среза.
};

/**
 * @brief Прочитать ограниченный срез файла любого типа.
 *
 * @param file Путь к файлу.
 * @param limits Ограничения среза.
 * @param out_slice Выходной текст среза.
 * @param out_truncated Признак усечения по лимиту.
 * @param out_error Причина сбоя.
 * @return true при успехе.
 */
bool read_file_slice(const std::filesystem::path& file,
                     const SFileSliceLimits& limits,
                     std::string& out_slice,
                     bool& out_truncated,
                     std::string& out_error);

/**
 * @brief Поставщик фактов о кодовой базе.
 */
class IFactProvider
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~IFactProvider() = default;

    /**
     * @brief Собрать факты по сущности.
     * @param request Запрос с именем и позицией сущности.
     * @return Секции фактов либо типизированный сбой.
     */
    virtual SFactResult collect(const SFactRequest& request) const = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_IFACTPROVIDER_H
