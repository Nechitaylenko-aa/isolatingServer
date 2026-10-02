#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cpptool
{

/**
 * @brief RAII-обёртка над соединением SQLite.
 *
 * Отвечает только за соединение и выполнение SQL: не знает ни про кэш TU,
 * ни про libclang. Все значения биндятся через подготовленные выражения —
 * SQL никогда не собирается конкатенацией строк, потому что в него попадают
 * пути файлов из проекта.
 */
class CDbManager
{
public:
    CDbManager() = default;
    ~CDbManager();

    CDbManager(const CDbManager&) = delete;
    CDbManager& operator=(const CDbManager&) = delete;
    CDbManager(CDbManager&& other) noexcept;
    CDbManager& operator=(CDbManager&& other) noexcept;

    /**
     * @brief Открывает (при необходимости создаёт) файл БД.
     *
     * Побочные эффекты: создаёт родительские каталоги пути, выставляет
     * journal_mode=WAL, busy_timeout и synchronous=NORMAL.
     *
     * @param path путь к файлу БД
     * @return true при успехе; при неудаче last_error() содержит причину
     */
    bool open(const std::string& path);

    /**
     * @brief Закрывает соединение. Идемпотентно.
     */
    void close();

    /**
     * @brief true, если соединение открыто и готово к работе.
     */
    bool is_open() const { return db_ != nullptr; }

    /**
     * @brief Выполняет SQL без результата (DDL, PRAGMA, INSERT без параметров).
     *
     * @param sql запрос
     * @return true при успехе; при неудаче last_error() содержит причину
     */
    bool execute(const std::string& sql);

    /**
     * @brief Выполняет запрос и возвращает единственное целое из первой строки.
     *
     * @param sql запрос
     * @param out значение; при отсутствии строки остаётся нетронутым
     * @return true, если строка была получена
     */
    bool query_int(const std::string& sql, int64_t& out);

    /**
     * @brief Выполняет запрос и возвращает единственную строку из первой строки.
     *
     * @param sql запрос
     * @param out значение; при отсутствии строки остаётся нетронутым
     * @return true, если строка была получена
     */
    bool query_string(const std::string& sql, std::string& out);

    /**
     * @brief Начинает транзакцию.
     * @return true при успехе
     */
    bool begin_transaction();

    /**
     * @brief Фиксирует транзакцию.
     * @return true при успехе
     */
    bool commit();

    /**
     * @brief Откатывает транзакцию.
     *
     * Откат не считается ошибкой, если транзакция уже завершена — метод
     * предназначен для вызова из обработчиков ошибок, где состояние
     * транзакции может быть любым.
     *
     * @return true при успехе
     */
    bool rollback();

    /**
     * @brief Текст последней ошибки SQLite.
     */
    const std::string& last_error() const { return lastError_; }

    /**
     * @brief accessor: сырой handle, нужен вызывающему коду для prepare.
     */
    sqlite3* handle() const { return db_; }

private:
    bool set_error_from_db(const std::string& context);
    void apply_open_pragmas();

    sqlite3* db_{nullptr};
    std::string lastError_;
};

/**
 * @brief RAII-обёртка над подготовленным выражением SQLite.
 *
 * Значения биндятся 1-based индексами, ровно как в API SQLite.
 */
class CDbStatement
{
public:
    CDbStatement() = default;
    ~CDbStatement();

    CDbStatement(const CDbStatement&) = delete;
    CDbStatement& operator=(const CDbStatement&) = delete;
    CDbStatement(CDbStatement&& other) noexcept;
    CDbStatement& operator=(CDbStatement&& other) noexcept;

    /**
     * @brief Готовит выражение на соединении.
     *
     * @param db открытое соединение
     * @param sql запрос
     * @return true при успехе
     */
    bool prepare(sqlite3* db, const std::string& sql);

    /**
     * @brief Привязывает целое к параметру с индексом 1-based.
     * @return true при успехе
     */
    bool bind_int(int index, int64_t value);

    /**
     * @brief Привязывает строку к параметру с индексом 1-based.
     * @return true при успехе
     */
    bool bind_string(int index, const std::string& value);

    /**
     * @brief Выполняет шаг выражения.
     *
     * @return true, если доступна строка (SQLITE_ROW или SQLITE_DONE с данными
     *         предыдущего шага); false при ошибке или завершении без данных
     */
    bool step();

    /**
     * @brief true, если последний step() вернул строку, а не завершение.
     */
    bool has_row() const { return hasRow_; }

    /**
     * @brief Читает целое из колонки 0-based текущей строки.
     */
    int64_t column_int(int index) const;

    /**
     * @brief Читает строку из колонки 0-based текущей строки.
     */
    std::string column_string(int index) const;

    /**
     * @brief Сбрасывает выражение для повторного выполнения с новыми bind.
     * @return true при успехе
     */
    bool reset();

    /**
     * @brief Текст последней ошибки.
     */
    const std::string& last_error() const { return lastError_; }

private:
    sqlite3_stmt* stmt_{nullptr};
    bool hasRow_{false};
    std::string lastError_;
};

} // namespace cpptool
