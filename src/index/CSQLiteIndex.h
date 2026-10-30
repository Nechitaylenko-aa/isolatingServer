/**
 * @file CSQLiteIndex.h
 * @brief SQLite-реализация индекса проекта (ADR-02, ADR-03).
 *
 * Управляет соединением с конкретной index.db текущего проекта через нативный
 * C-API sqlite3. Перед изменением схемы или данных создаёт резервную копию
 * index.db.bak посредством sqlite3_backup. Чтение/запись соединения используют
 * PRAGMA journal_mode=WAL и иностранные ключи.
 */

#ifndef FCPP_AGENT_CSQLITEINDEX_H
#define FCPP_AGENT_CSQLITEINDEX_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "fcpp-agent/IClang.h"
#include "fcpp-agent/IIndex.h"
#include "fcpp-agent/SCompileCommand.h"
#include "fcpp-agent/SStaleReport.h"

struct sqlite3;

namespace fcpp
{

/**
 * @brief Индекс проекта, хранимый в базе данных SQLite.
 *
 * Путь к index.db передаётся в конструкторе (детерминизм мест): его определяет
 * Project Manager по каноническому пути проекта (ADR-03). Класс не выполняет
 * поиск базы данных по дереву проекта.
 */
class CSQLiteIndex final : public IIndex
{
public:
    /**
     * @brief Конструктор с детерминированным путём к базе индекса.
     * @param db_path Путь к файлу index.db текущего проекта.
     */
    explicit CSQLiteIndex(std::filesystem::path db_path);

    /**
     * @brief Деструктор: закрывает соединение.
     */
    ~CSQLiteIndex() override;

    /**
     * @brief Копирование запрещено (владение соединением sqlite3).
     */
    CSQLiteIndex(const CSQLiteIndex&) = delete;

    /**
     * @brief Присваивание запрещено.
     * @return Ссылка на текущий объект.
     */
    CSQLiteIndex& operator=(const CSQLiteIndex&) = delete;

    /**
     * @brief Открыть соединение с базой индекса и включить PRAGMA режимы.
     * @return true при успехе; false при ошибке открытия.
     */
    bool open_db();

    /**
     * @brief Закрыть соединение с базой индекса.
     * @return true при успехе; false при ошибке закрытия.
     */
    bool close_db();

    /**
     * @brief Создать резервную копию базы индекса через sqlite3_backup.
     * @param backup_path Путь к создаваемому файлу резервной копии.
     * @return true при успехе; false при ошибке.
     */
    bool backup_database(const std::filesystem::path& backup_path) const;

    /**
     * @brief Сохранить инвентаризацию translation units в новую generation.
     *
     * Создаёт новую generation, записывает файлы (путь, mtime, size) и команды
     * компиляции, активирует generation и помечает предыдущую как RETIRED.
     * Выполняется в транзакции BEGIN IMMEDIATE; при ошибке — ROLLBACK.
     * @param commands Команды компиляции из compile_commands.json.
     * @return true при успехе; false при ошибке.
     */
    bool store_inventory(const std::vector<SCompileCommand>& commands);

    /**
     * @brief Прогреть индекс: разобрать TU и наполнить таблицы символов и связей.
     *
     * Использует переданный backend IClang для разбора translation units,
     * затем сохраняет symbol/symbol_location/include/inheritance/override/
     * reference/call в рамках одной транзакции. При full=false выполняется
     * точечная переиндексация указанных TU и их заголовков (Этап 4.1).
     * @param clang Backend доступа к libclang.
     * @param reindex_paths Пути для точечной переиндексации (при full=false).
     * @param full Полная переиндексация generation (по умолчанию true).
     * @return true при успехе; false при ошибке.
     */
    bool warm(IClang& clang,
              const std::vector<std::string>& reindex_paths = {},
              bool full = true);

    /**
     * @brief Открыть индекс для проекта.
     * @param project_root Канонический путь CMake-корня проекта.
     * @return true при успешном открытии.
     */
    bool open(const std::filesystem::path& project_root) override;

    /**
     * @brief Закрыть индекс и освободить ресурсы.
     */
    void close() override;

    /**
     * @brief Получить снимок состояния индекса.
     * @return Структура состояния без загрузки данных.
     */
    SIndexStatus status() const override;

    /**
     * @brief Инкрементально обновить устаревшие translation units.
     * @return true при успешном обновлении; false при ошибке.
     */
    bool refresh(IClang& clang) override;

    /**
     * @brief Полностью перестроить индекс в новую generation.
     * @return true при успешном построении; false при ошибке.
     */
    bool rebuild(IClang& clang, const std::vector<SCompileCommand>& commands) override;

    /**
     * @brief Определить устаревание индекса (ADR-05, Этап 4.1).
     * @param external_compile_commands Путь к compile_commands.json (может быть пустым).
     * @param out_report Выходной отчёт.
     * @return true при успешной проверке.
     */
    bool detect_stale(const std::string& external_compile_commands, SStaleReport& out_report) const;

    /**
     * @brief Обновить индекс, если он устарел (точечно или полным rebuild).
     *
     * При полном rebuild используется переданный инвентарь, а не сохранённый в
     * активном поколении: если устаревание вызвано появлением нового
     * translation unit, старый инвентарь его не содержит и индекс снова
     * оказался бы устаревшим.
     *
     * @param clang Backend доступа к libclang.
     * @param external_compile_commands Путь к compile_commands.json (может быть пустым).
     * @param fresh_commands Актуальный инвентарь с диска (пусто — взять сохранённый).
     * @return true при успехе; false при ошибке.
     */
    bool refresh_if_stale(IClang& clang, const std::string& external_compile_commands,
                          const std::vector<SCompileCommand>& fresh_commands = {});

    /**
     * @brief Сохранить отпечаток compile_commands.json для детекции изменений окружения.
     * @param compile_commands_path Путь к compile_commands.json.
     */
    void set_compile_commands_fingerprint(const std::string& compile_commands_path);

    /**
     * @brief Найти сущность по стабильному идентификатору clang (USR).
     * @param usr Идентификатор сущности.
     * @return Сущность либо пустое значение.
     */
    std::optional<SSymbol> find_symbol_by_usr(const std::string& usr) const override;

    /**
     * @brief Найти сущности по короткому/полному имени.
     * @param name Имя сущности.
     * @return Список совпавших сущностей.
     */
    std::vector<SSymbol> find_symbols_by_name(const std::string& name) const override;

    /**
     * @brief Получить структуру сущности (для класса — методы и поля).
     * @param usr Идентификатор сущности.
     * @return Дочерние сущности.
     */
    std::vector<SSymbol> outline(const std::string& usr) const override;

    /**
     * @brief Получить функции/методы, вызывающие указанную сущность.
     * @param usr Идентификатор сущности.
     * @return Список вызывающих сущностей.
     */
    std::vector<SSymbol> callers(const std::string& usr) const override;

    /**
     * @brief Получить сущности, вызываемые указанной функцией/методом.
     * @param usr Идентификатор сущности.
     * @return Список вызываемых сущностей.
     */
    std::vector<SSymbol> callees(const std::string& usr) const override;

    /**
     * @brief Получить места использования сущности.
     * @param usr Идентификатор сущности.
     * @return Список позиций использования.
     */
    std::vector<SSymbolLocation> references(const std::string& usr) const override;

    /**
     * @brief Получить минимальный срез исходного кода сущности.
     * @param usr Идентификатор сущности.
     * @return Срез кода либо пустое значение.
     */
    std::optional<SSourceSlice> get_source_slice(const std::string& usr) const override;

    /**
     * @brief Найти идентификатор файла по пути в активной generation.
     * @param path Путь к файлу.
     * @return Идентификатор файла либо пустое значение.
     */
    std::optional<long long> find_file_id(const std::string& path) const;

    /**
     * @brief Получить дескриптор открытого соединения.
     * @return Указатель sqlite3 (nullptr если не открыто).
     */
    sqlite3* handle() const;

    /**
     * @brief Прочитать аргументы компиляции файла в активной generation.
     * @param path Путь к файлу.
     * @param out_arguments Выходные аргументы.
     * @return true при успехе.
     */
    bool load_file_arguments(const std::string& path, std::vector<std::string>& out_arguments) const;

private:
    /**
     * @brief Выполнить SQL-выражение без результата на текущем соединении.
     * @param sql Текст SQL.
     * @return true при успехе.
     */
    bool exec_sql(const char* sql) const;

    /**
     * @brief Получить номер активной generation.
     * @return Номер generation (0 если активной нет).
     */
    long long active_generation() const;

    /**
     * @brief Удалить все строки generation (символы, связи, файлы, команды).
     * @param generation Номер generation.
     * @return true при успехе.
     */
    bool delete_generation_data(long long generation);

    /**
     * @brief Прочитать команды компиляции активной generation.
     * @param out_commands Выходной список команд.
     * @return true при успехе.
     */
    bool load_current_commands(std::vector<SCompileCommand>& out_commands) const;

    /**
     * @brief Удалить строки всех RETIRED-поколений (данные и записи поколений).
     * @return true при успехе.
     */
    bool purge_retired_generations();

    /**
     * @brief Пересчитать и сохранить content-hash для всех файлов generation.
     */
    void compute_content_hashes() const;

    /**
     * @brief Список путей файлов активной generation.
     * @return Пути файлов.
     */
    std::vector<std::string> file_paths() const;

    /**
     * @brief Найти TU, транзитивно включающие заголовок.
     * @param header_path Путь заголовка.
     * @param out_tus Выходное множество TU (дополняется).
     */
    void include_dependents(const std::string& header_path, std::set<std::string>& out_tus) const;

    /**
     * @brief Извлечь пути TU из внешнего compile_commands.json.
     * @param external_compile_commands Путь к файлу (может быть пустым).
     * @return Пути translation units.
     */
    std::vector<std::string> external_tu_paths(const std::string& external_compile_commands) const;

    /**
     * @brief Пометить исчезнувшие файлы (mtime=size=0, пустой hash).
     * @param removed_files Пути исчезнувших файлов.
     */
    void clear_removed_files(const std::vector<std::string>& removed_files);

    /**
     * @brief Переустановить parent_id и inheritance.base_id по USR/имени после точечного обновления.
     * @param generation Номер generation.
     */
    void relink_parent_ids(long long generation);

    sqlite3* m_db;                             ///< Открытое соединение SQLite.
    std::filesystem::path m_db_path;           ///< Путь к index.db.
    std::filesystem::path m_project_root;      ///< Канонический путь проекта.
};

} // namespace fcpp

#endif // FCPP_AGENT_CSQLITEINDEX_H
