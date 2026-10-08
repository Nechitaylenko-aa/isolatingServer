/**
 * @file CSchemaMigrator.h
 * @brief Детерминированный движок миграций схемы SQLite (ADR-02).
 *
 * Накатывает нумерованные .sql-файлы из каталога migrations/ на базу данных
 * в порядке возрастания версии. Каждая миграция применяется в отдельной
 * транзакции BEGIN IMMEDIATE; при ошибке выполняется ROLLBACK и возврат false.
 * Каталог миграций задаётся на этапе сборки (FCPP_AGENT_MIGRATIONS_DIR) и
 * может быть переопределён переменной окружения FCPP_AGENT_MIGRATIONS.
 */

#ifndef FCPP_AGENT_CSCHEMAMIGRATOR_H
#define FCPP_AGENT_CSCHEMAMIGRATOR_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

struct sqlite3;

namespace fcpp
{

/**
 * @brief Движок миграций схемы SQLite.
 *
 * Без внутреннего состояния: каждая операция принимает путь к БД.
 * Транзакционность и откат обеспечиваются нативным C-API sqlite3.
 */
class CSchemaMigrator
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CSchemaMigrator() = default;

    /**
     * @brief Деструктор по умолчанию.
     */
    ~CSchemaMigrator() = default;

    /**
     * @brief Копирование запрещено (нет внутреннего состояния для дублирования).
     */
    CSchemaMigrator(const CSchemaMigrator&) = delete;

    /**
     * @brief Присваивание запрещено.
     * @return Ссылка на текущий объект.
     */
    CSchemaMigrator& operator=(const CSchemaMigrator&) = delete;

    /**
     * @brief Применить все неприменённые миграции к базе данных.
     *
     * Открывает соединение, включает PRAGMA journal_mode=WAL и foreign_keys=ON,
     * затем для каждой миграции версии > текущей выполняет
     * BEGIN IMMEDIATE, применяет SQL, обновляет версию в schema_meta и делает
     * COMMIT. При ошибке — ROLLBACK и возврат false.
     * @param db_path Путь к файлу базы данных SQLite.
     * @return true при успехе; false при любой ошибке.
     */
    bool migrate(const std::string& db_path) const;

    /**
     * @brief Прочитать текущую версию схемы из schema_meta.
     * @param db_path Путь к файлу базы данных SQLite.
     * @return Версия схемы (0, если таблица отсутствует или пуста).
     */
    std::uint32_t schema_version(const std::string& db_path) const;

private:
    /**
     * @brief Собрать список миграций, отсортированный по версии.
     * @return Пары (версия, путь к SQL-файлу).
     */
    static std::vector<std::pair<std::uint32_t, std::filesystem::path>> collect_migrations();

    /**
     * @brief Извлечь номер версии из имени файла миграции.
     * @param path Путь к файлу миграции.
     * @return Числовой префикс имени (0 при отсутствии).
     */
    static std::uint32_t parse_version(const std::filesystem::path& path);

    /**
     * @brief Прочитать содержимое файла целиком.
     * @param path Путь к файлу.
     * @param out_content Выходное содержимое.
     * @return true при успешном чтении.
     */
    static bool read_file(const std::filesystem::path& path, std::string& out_content);

    /**
     * @brief Выполнить SQL-выражение без получения результата.
     * @param db Открытое соединение SQLite.
     * @param sql Текст SQL.
     * @return true при успехе.
     */
    static bool exec_sql(sqlite3* db, const char* sql);

    /**
     * @brief Прочитать версию схемы из соединения.
     * @param db Открытое соединение SQLite.
     * @param out_version Выходная версия.
     * @return true при успехе.
     */
    static bool read_schema_version(sqlite3* db, std::uint32_t& out_version);
};

} // namespace fcpp

#endif // FCPP_AGENT_CSCHEMAMIGRATOR_H
