/**
 * @file CSQLiteIndex.cpp
 * @brief Реализация SQLite-индекса проекта (ADR-02, ADR-03, ADR-11).
 */

#include "index/CSQLiteIndex.h"

#include <algorithm>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "core/CHash.h"

namespace fcpp
{

namespace
{

/** @brief Таймаут ожидания снятия блокировки SQLite (мс). */
constexpr int k_busy_timeout_ms = 10000;

/** @brief Суффикс файла резервной копии индекса. */
constexpr const char* k_backup_suffix = ".bak";

/** @brief SQL-подзапрос активной generation. */
constexpr const char* k_active_generation =
    "(SELECT generation FROM index_generation WHERE state = 'ACTIVE' ORDER BY generation DESC LIMIT 1)";

/**
 * @brief Является ли файл внешним (системный/сторонний) — не подлежит индексации (§15).
 *
 * Отсекаем системные пути, зависимости FetchContent (`_deps`) и всё, что лежит
 * вне корня проекта. Иначе индекс засоряется символами сторонних библиотек.
 * @param path Путь к файлу.
 * @param project_root Корень проекта (может быть пустым).
 * @return true если файл внешний.
 */
bool is_external_path(const std::string& path, const std::string& project_root)
{
    if (path.empty())
    {
        return true;
    }
    if (path.rfind("/usr/", 0) == 0 || path.rfind("/opt/", 0) == 0
            || path.rfind("/etc/", 0) == 0 || path.rfind("/Library/", 0) == 0)
    {
        return true;
    }
    if (path.find("/_deps/") != std::string::npos || path.find("/build/_deps/") != std::string::npos)
    {
        return true;
    }
    // Абсолютный путь вне корня проекта — внешний.
    if (!project_root.empty() && path.front() == '/' && path.rfind(project_root, 0) != 0)
    {
        return true;
    }
    return false;
}

/** @brief Имена категорий символов, индексированные по ESymbolKind (Секция 3). */
static const char* const m_symbol_kind_names[static_cast<std::size_t>(ESymbolKind::Count)]
{
    "namespace",
    "class",
    "struct",
    "union",
    "enum",
    "enum_constant",
    "typedef",
    "function",
    "method",
    "constructor",
    "destructor",
    "variable",
    "field",
    "parameter"
};

/**
 * @brief Восстановить ESymbolKind из строкового имени.
 * @param name Имя категории.
 * @return Категория (Namespace при неизвестном значении).
 */
ESymbolKind symbol_kind_from_name(const std::string& name)
{
    for (std::size_t index = 0; index < static_cast<std::size_t>(ESymbolKind::Count); ++index)
    {
        if (name == m_symbol_kind_names[index])
        {
            return static_cast<ESymbolKind>(index);
        }
    }
    return ESymbolKind::Namespace;
}

/**
 * @brief Прочитать текстовую колонку.
 * @param statement Подготовленное выражение.
 * @param index Индекс колонки.
 * @return Строка (пустая для NULL).
 */
std::string read_text(sqlite3_stmt* statement, int index)
{
    const unsigned char* text = sqlite3_column_text(statement, index);
    return (text != nullptr) ? reinterpret_cast<const char*>(text) : std::string();
}

/**
 * @brief Вычислить SHA-256 содержимого файла.
 * @param path Путь к файлу.
 * @param out_hash Выходной hex-дайджест.
 * @return true при успехе.
 */
bool compute_file_hash(const std::filesystem::path& path, std::string& out_hash)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    out_hash = CHash::sha256_hex(buffer.str());
    return true;
}

/**
 * @brief Получить mtime файла в unix-секундах.
 * @param path Путь к файлу.
 * @return mtime (0 при ошибке).
 */
std::int64_t file_mtime(const std::filesystem::path& path)
{
    std::error_code error_code;
    const auto file_time = std::filesystem::last_write_time(path, error_code);
    if (error_code)
    {
        return 0;
    }
    return std::chrono::duration_cast<std::chrono::seconds>(file_time.time_since_epoch()).count();
}

/**
 * @brief Получить размер файла.
 * @param path Путь к файлу.
 * @return Размер (0 при ошибке).
 */
std::uint64_t file_size(const std::filesystem::path& path)
{
    std::error_code error_code;
    const std::uint64_t size = std::filesystem::file_size(path, error_code);
    return error_code ? 0u : size;
}

/**
 * @brief Прочитать значение таблицы schema_meta.
 * @param db Соединение.
 * @param key Ключ.
 * @return Значение либо пустая строка.
 */
std::string read_schema_meta(sqlite3* db, const char* key)
{
    if (db == nullptr || key == nullptr)
    {
        return {};
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT value FROM schema_meta WHERE key = ?1;", -1, &statement, nullptr) != SQLITE_OK)
    {
        return {};
    }
    sqlite3_bind_text(statement, 1, key, -1, SQLITE_TRANSIENT);

    std::string value;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        value = read_text(statement, 0);
    }
    sqlite3_finalize(statement);
    return value;
}

/**
 * @brief Задача индексирования одного translation unit.
 */
struct STuJob
{
    sqlite3_int64 m_file_id;              ///< Идентификатор файла в индексе.
    std::string m_path;                   ///< Путь к исходному файлу.
    std::vector<std::string> m_arguments; ///< Аргументы компиляции.
};

/**
 * @brief Стейджинговая запись символа (до вставки в БД).
 */
struct SStagedSymbol
{
    std::string m_usr;
    ESymbolKind m_kind;
    std::string m_name;
    std::string m_qualified_name;
    std::string m_signature;
    std::string m_access;
    bool m_is_definition;
    bool m_is_virtual;
    bool m_is_pure_virtual;
    bool m_is_static;
    std::string m_parent_usr;
    std::string m_file;
    std::uint32_t m_line;
    std::uint32_t m_column;
    std::uint32_t m_end_line;
    std::uint32_t m_end_column;
};

/**
 * @brief Стейджинговая запись наследования.
 */
struct SStagedBase
{
    std::string m_derived_usr;
    std::string m_base_usr;
    std::string m_base_qname;
    std::string m_access;
    bool m_is_virtual;
};

/**
 * @brief Стейджинговая запись переопределения.
 */
struct SStagedOverride
{
    std::string m_method_usr;
    std::string m_base_method_usr;
};

/**
 * @brief Стейджинговая запись ссылки.
 */
struct SStagedReference
{
    std::string m_symbol_usr;
    std::string m_kind;
    std::string m_file;
    std::uint32_t m_line;
    std::uint32_t m_column;
};

/**
 * @brief Стейджинговая запись вызова.
 */
struct SStagedCall
{
    std::string m_caller_usr;
    std::string m_callee_usr;
    std::string m_callee_qname;
    std::string m_file;
    std::uint32_t m_line;
    std::uint32_t m_column;
};

/**
 * @brief Стейджинговая запись include.
 */
struct SStagedInclude
{
    std::string m_from_file;
    std::string m_to_path;
    bool m_is_angle;
};

/**
 * @brief Разбить текст аргументов на элементы по разделителю '\n'.
 * @param text Текст.
 * @return Элементы.
 */
std::vector<std::string> split_arguments(const std::string& text)
{
    std::vector<std::string> items;
    std::string current;
    for (const char character : text)
    {
        if (character == '\n')
        {
            if (!current.empty())
            {
                items.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(character);
        }
    }
    if (!current.empty())
    {
        items.push_back(current);
    }
    return items;
}

/**
 * @brief Собрать строку аргументов из элементов (разделитель '\n').
 * @param items Элементы.
 * @return Текст.
 */
std::string join_arguments(const std::vector<std::string>& items)
{
    std::string result;
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (index > 0)
        {
            result.push_back('\n');
        }
        result.append(items[index]);
    }
    return result;
}

} // namespace

CSQLiteIndex::CSQLiteIndex(std::filesystem::path db_path)
    : m_db(nullptr)
    , m_db_path(std::move(db_path))
{
}

CSQLiteIndex::~CSQLiteIndex()
{
    close_db();
}

bool CSQLiteIndex::exec_sql(const char* sql) const
{
    if (m_db == nullptr || sql == nullptr)
    {
        return false;
    }

    char* error_message = nullptr;
    const int result_code = sqlite3_exec(m_db, sql, nullptr, nullptr, &error_message);
    if (error_message != nullptr)
    {
        sqlite3_free(error_message);
    }

    return result_code == SQLITE_OK;
}

bool CSQLiteIndex::open_db()
{
    if (m_db != nullptr)
    {
        return true;
    }

    if (m_db_path.empty())
    {
        return false;
    }

    if (sqlite3_open(m_db_path.string().c_str(), &m_db) != SQLITE_OK)
    {
        if (m_db != nullptr)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    sqlite3_busy_timeout(m_db, k_busy_timeout_ms);

    if (!exec_sql("PRAGMA journal_mode=WAL;"))
    {
        close_db();
        return false;
    }

    if (!exec_sql("PRAGMA foreign_keys=ON;"))
    {
        close_db();
        return false;
    }

    return true;
}

bool CSQLiteIndex::close_db()
{
    if (m_db == nullptr)
    {
        return true;
    }

    const int result_code = sqlite3_close(m_db);
    m_db = nullptr;
    return result_code == SQLITE_OK;
}

bool CSQLiteIndex::backup_database(const std::filesystem::path& backup_path) const
{
    if (m_db == nullptr || backup_path.empty())
    {
        return false;
    }

    sqlite3* backup_db = nullptr;
    if (sqlite3_open(backup_path.string().c_str(), &backup_db) != SQLITE_OK)
    {
        if (backup_db != nullptr)
        {
            sqlite3_close(backup_db);
        }
        return false;
    }

    bool success = true;
    sqlite3_backup* backup = sqlite3_backup_init(backup_db, "main", m_db, "main");
    if (backup == nullptr)
    {
        success = false;
    }
    else
    {
        const int step_result = sqlite3_backup_step(backup, -1);
        const int finish_result = sqlite3_backup_finish(backup);
        if (step_result != SQLITE_DONE || finish_result != SQLITE_OK)
        {
            success = false;
        }
    }

    sqlite3_close(backup_db);
    return success;
}

bool CSQLiteIndex::open(const std::filesystem::path& project_root)
{
    m_project_root = project_root;
    return open_db();
}

void CSQLiteIndex::close()
{
    close_db();
}

SIndexStatus CSQLiteIndex::status() const
{
    SIndexStatus result;
    result.m_state = EIndexState::NotReady;
    result.m_generation = 0;
    result.m_file_count = 0;
    result.m_symbol_count = 0;
    result.m_stale_files = 0;

    if (m_db == nullptr)
    {
        return result;
    }

    std::uint64_t active_generation = 0;
    {
        const char* sql =
            "SELECT generation FROM index_generation "
            "WHERE state = 'ACTIVE' ORDER BY generation DESC LIMIT 1;";

        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(statement) == SQLITE_ROW)
            {
                active_generation = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 0));
            }
            sqlite3_finalize(statement);
        }
    }

    if (active_generation == 0)
    {
        return result;
    }

    auto count_rows = [this](const char* sql, std::uint64_t generation) -> std::uint64_t
    {
        std::uint64_t count = 0;
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(generation));
            if (sqlite3_step(statement) == SQLITE_ROW)
            {
                count = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 0));
            }
            sqlite3_finalize(statement);
        }
        return count;
    };

    result.m_state = EIndexState::Ready;
    result.m_generation = active_generation;
    result.m_file_count = count_rows("SELECT COUNT(*) FROM file WHERE generation = ?1;", active_generation);
    result.m_symbol_count = count_rows("SELECT COUNT(*) FROM symbol WHERE generation = ?1;", active_generation);

    return result;
}

bool CSQLiteIndex::store_inventory(const std::vector<SCompileCommand>& commands)
{
    if (m_db == nullptr)
    {
        return false;
    }

    if (!exec_sql("BEGIN IMMEDIATE;"))
    {
        return false;
    }

    sqlite3_int64 generation = 0;
    {
        sqlite3_stmt* statement = nullptr;
        const char* sql = "SELECT COALESCE(MAX(generation), 0) + 1 FROM index_generation;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            generation = sqlite3_column_int64(statement, 0);
        }
        sqlite3_finalize(statement);
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const sqlite3_int64 timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now).count();

    {
        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "INSERT INTO index_generation (generation, state, started_at, finished_at, file_count, symbol_count) "
            "VALUES (?1, 'ACTIVE', ?2, ?2, ?3, 0);";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        sqlite3_bind_int64(statement, 2, timestamp);
        sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(inventory_commands.size()));
        const bool inserted = (sqlite3_step(statement) == SQLITE_DONE);
        sqlite3_finalize(statement);
        if (!inserted)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
    }

    {
        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "UPDATE index_generation SET state = 'RETIRED' "
            "WHERE state = 'ACTIVE' AND generation <> ?1;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        const bool updated = (sqlite3_step(statement) == SQLITE_DONE);
        sqlite3_finalize(statement);
        if (!updated)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
    }

    // Одна compile command на путь: один TU может повторяться для нескольких targets.
    // Сохраняем первое вхождение в compile_commands.json и нормализуем путь.
    std::vector<const SCompileCommand*> inventory_commands;
    std::unordered_set<std::string> seen_paths;
    inventory_commands.reserve(commands.size());
    for (const SCompileCommand& command : commands)
    {
        const std::string normalized_path = command.m_file.lexically_normal().string();
        if (is_external_path(normalized_path, m_project_root.string()))
        {
            continue;
        }
        if (!seen_paths.insert(normalized_path).second)
        {
            continue;
        }
        inventory_commands.push_back(&command);
    }

    for (const SCompileCommand* command_ptr : inventory_commands)
    {
        const SCompileCommand& command = *command_ptr;

        std::error_code error_code;
        const std::uint64_t size = std::filesystem::file_size(command.m_file, error_code);
        const std::uint64_t resolved_size = error_code ? 0u : size;

        sqlite3_int64 mtime = 0;
        if (!error_code)
        {
            const auto file_time = std::filesystem::last_write_time(command.m_file, error_code);
            if (!error_code)
            {
                mtime = std::chrono::duration_cast<std::chrono::seconds>(file_time.time_since_epoch()).count();
            }
        }

        const std::string path = command.m_file.lexically_normal().string();

        sqlite3_stmt* file_statement = nullptr;
        const char* file_sql =
            "INSERT INTO file (path, generation, mtime, size) VALUES (?1, ?2, ?3, ?4);";
        if (sqlite3_prepare_v2(m_db, file_sql, -1, &file_statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_text(file_statement, 1, path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(file_statement, 2, generation);
        sqlite3_bind_int64(file_statement, 3, mtime);
        sqlite3_bind_int64(file_statement, 4, static_cast<sqlite3_int64>(resolved_size));
        const bool file_inserted = (sqlite3_step(file_statement) == SQLITE_DONE);
        sqlite3_finalize(file_statement);
        if (!file_inserted)
        {
            exec_sql("ROLLBACK;");
            return false;
        }

        const sqlite3_int64 file_id = sqlite3_last_insert_rowid(m_db);

        const std::string arguments = join_arguments(command.m_arguments).empty()
                                          ? command.m_command
                                          : join_arguments(command.m_arguments);
        const std::string language = command.m_language.empty() ? std::string("c++") : command.m_language;

        sqlite3_stmt* command_statement = nullptr;
        const char* command_sql =
            "INSERT INTO compile_command (file_id, directory, arguments, language) VALUES (?1, ?2, ?3, ?4);";
        if (sqlite3_prepare_v2(m_db, command_sql, -1, &command_statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        const std::string directory = command.m_directory.string();
        sqlite3_bind_int64(command_statement, 1, file_id);
        sqlite3_bind_text(command_statement, 2, directory.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(command_statement, 3, arguments.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(command_statement, 4, language.c_str(), -1, SQLITE_TRANSIENT);
        const bool command_inserted = (sqlite3_step(command_statement) == SQLITE_DONE);
        sqlite3_finalize(command_statement);
        if (!command_inserted)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
    }

    if (!exec_sql("COMMIT;"))
    {
        exec_sql("ROLLBACK;");
        return false;
    }

    return true;
}

sqlite3* CSQLiteIndex::handle() const
{
    return m_db;
}

bool CSQLiteIndex::load_file_arguments(const std::string& path,
                                       std::vector<std::string>& out_arguments) const
{
    out_arguments.clear();
    if (m_db == nullptr || path.empty())
    {
        return false;
    }

    const std::string sql = std::string(
        "SELECT c.arguments FROM compile_command c JOIN file f ON f.id = c.file_id "
        "WHERE f.path = ?1 AND f.generation = ") + k_active_generation + " LIMIT 1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }
    sqlite3_bind_text(statement, 1, path.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        out_arguments = split_arguments(read_text(statement, 0));
        found = true;
    }
    sqlite3_finalize(statement);
    return found;
}

std::optional<long long> CSQLiteIndex::find_file_id(const std::string& path) const
{
    if (m_db == nullptr || path.empty())
    {
        return std::nullopt;
    }

    const std::string sql = std::string("SELECT id FROM file WHERE path = ?1 AND generation = ")
                            + k_active_generation + " LIMIT 1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }
    sqlite3_bind_text(statement, 1, path.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<long long> result;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        result = sqlite3_column_int64(statement, 0);
    }
    sqlite3_finalize(statement);
    return result;
}

long long CSQLiteIndex::active_generation() const
{
    if (m_db == nullptr)
    {
        return 0;
    }

    const char* sql =
        "SELECT generation FROM index_generation WHERE state = 'ACTIVE' ORDER BY generation DESC LIMIT 1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return 0;
    }

    long long generation = 0;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        generation = sqlite3_column_int64(statement, 0);
    }
    sqlite3_finalize(statement);
    return generation;
}

bool CSQLiteIndex::delete_generation_data(long long generation)
{
    if (m_db == nullptr)
    {
        return false;
    }

    // Полное удаление данных поколения (используется при очистке RETIRED).
    const char* child_queries[] =
    {
        "DELETE FROM symbol_location WHERE symbol_id IN (SELECT id FROM symbol WHERE generation = ?1);",
        "DELETE FROM reference WHERE generation = ?1;",
        "DELETE FROM call WHERE generation = ?1;",
        "DELETE FROM override WHERE generation = ?1;",
        "DELETE FROM inheritance WHERE generation = ?1;",
        "DELETE FROM include WHERE generation = ?1;",
        "DELETE FROM symbol WHERE generation = ?1;",
        "DELETE FROM compile_command WHERE file_id IN (SELECT id FROM file WHERE generation = ?1);",
        "DELETE FROM file WHERE generation = ?1;"
    };

    for (const char* query : child_queries)
    {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db, query, -1, &statement, nullptr) != SQLITE_OK)
        {
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        const bool done = (sqlite3_step(statement) == SQLITE_DONE);
        sqlite3_finalize(statement);
        if (!done)
        {
            return false;
        }
    }

    return true;
}

bool CSQLiteIndex::load_current_commands(std::vector<SCompileCommand>& out_commands) const
{
    out_commands.clear();
    if (m_db == nullptr)
    {
        return false;
    }

    const std::string sql = std::string(
        "SELECT f.path, c.directory, c.arguments, c.language FROM file f "
        "JOIN compile_command c ON c.file_id = f.id WHERE f.generation = ") + k_active_generation + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SCompileCommand command;
        command.m_file = read_text(statement, 0);
        command.m_directory = read_text(statement, 1);
        command.m_arguments = split_arguments(read_text(statement, 2));
        command.m_language = read_text(statement, 3);
        out_commands.push_back(std::move(command));
    }
    sqlite3_finalize(statement);
    return true;
}

bool CSQLiteIndex::purge_retired_generations()
{
    if (m_db == nullptr)
    {
        return false;
    }

    std::vector<long long> retired;
    {
        sqlite3_stmt* statement = nullptr;
        const char* sql = "SELECT generation FROM index_generation WHERE state = 'RETIRED';";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            return false;
        }
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            retired.push_back(sqlite3_column_int64(statement, 0));
        }
        sqlite3_finalize(statement);
    }

    if (retired.empty())
    {
        return true;
    }

    if (!exec_sql("BEGIN IMMEDIATE;"))
    {
        return false;
    }

    for (const long long generation : retired)
    {
        if (!delete_generation_data(generation))
        {
            exec_sql("ROLLBACK;");
            return false;
        }

        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db, "DELETE FROM index_generation WHERE generation = ?1;", -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        const bool done = (sqlite3_step(statement) == SQLITE_DONE);
        sqlite3_finalize(statement);
        if (!done)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
    }

    return exec_sql("COMMIT;");
}

bool CSQLiteIndex::refresh(IClang& clang)
{
    if (m_db == nullptr)
    {
        return false;
    }

    std::vector<SCompileCommand> commands;
    if (!load_current_commands(commands))
    {
        return false;
    }

    return rebuild(clang, commands);
}

bool CSQLiteIndex::rebuild(IClang& clang, const std::vector<SCompileCommand>& commands)
{
    if (m_db == nullptr)
    {
        return false;
    }

    const std::filesystem::path backup_path =
        std::filesystem::path(m_db_path.string() + k_backup_suffix);
    if (!backup_database(backup_path))
    {
        return false;
    }

    if (!store_inventory(commands))
    {
        return false;
    }

    if (!warm(clang))
    {
        return false;
    }

    purge_retired_generations();
    return true;
}

std::optional<SSymbol> CSQLiteIndex::find_symbol_by_usr(const std::string& usr) const
{
    if (m_db == nullptr || usr.empty())
    {
        return std::nullopt;
    }

    const char* sql =
        "SELECT usr, kind, qualified_name, name, signature, access, is_definition, flags, "
        "COALESCE((SELECT path FROM file WHERE id = symbol.file_id), '') "
        "FROM symbol WHERE usr = ?1 AND generation = "
        "(SELECT generation FROM index_generation WHERE state = 'ACTIVE' ORDER BY generation DESC LIMIT 1) LIMIT 1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }
    sqlite3_bind_text(statement, 1, usr.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<SSymbol> result;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        SSymbol symbol;
        symbol.m_usr = read_text(statement, 0);
        symbol.m_kind = symbol_kind_from_name(read_text(statement, 1));
        symbol.m_qualified_name = read_text(statement, 2);
        symbol.m_name = read_text(statement, 3);
        symbol.m_signature = read_text(statement, 4);
        symbol.m_access = read_text(statement, 5);
        symbol.m_is_definition = (sqlite3_column_int(statement, 6) != 0);
        const int flags = sqlite3_column_int(statement, 7);
        symbol.m_is_virtual = ((flags & 1) != 0);
        symbol.m_is_pure_virtual = ((flags & 2) != 0);
        symbol.m_is_static = ((flags & 4) != 0);
        symbol.m_location.m_file = read_text(statement, 8);
        result = std::move(symbol);
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<SSymbol> CSQLiteIndex::find_symbols_by_name(const std::string& name) const
{
    std::vector<SSymbol> result;
    if (m_db == nullptr || name.empty())
    {
        return result;
    }

    const char* sql =
        "SELECT usr, kind, qualified_name, name, signature, access, is_definition, flags, "
        "COALESCE((SELECT path FROM file WHERE id = symbol.file_id), '') "
        "FROM symbol WHERE name = ?1 AND generation = "
        "(SELECT generation FROM index_generation WHERE state = 'ACTIVE' ORDER BY generation DESC LIMIT 1);";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_text(statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SSymbol symbol;
        symbol.m_usr = read_text(statement, 0);
        symbol.m_kind = symbol_kind_from_name(read_text(statement, 1));
        symbol.m_qualified_name = read_text(statement, 2);
        symbol.m_name = read_text(statement, 3);
        symbol.m_signature = read_text(statement, 4);
        symbol.m_access = read_text(statement, 5);
        symbol.m_is_definition = (sqlite3_column_int(statement, 6) != 0);
        const int flags = sqlite3_column_int(statement, 7);
        symbol.m_is_virtual = ((flags & 1) != 0);
        symbol.m_is_pure_virtual = ((flags & 2) != 0);
        symbol.m_is_static = ((flags & 4) != 0);
        symbol.m_location.m_file = read_text(statement, 8);
        result.push_back(std::move(symbol));
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<SSymbol> CSQLiteIndex::outline(const std::string& usr) const
{
    std::vector<SSymbol> result;
    if (m_db == nullptr || usr.empty())
    {
        return result;
    }

    const std::string sql = std::string(
        "SELECT usr, kind, qualified_name, name, signature, access, is_definition, flags, "
        "COALESCE((SELECT path FROM file WHERE id = symbol.file_id), '') "
        "FROM symbol WHERE parent_id = (SELECT id FROM symbol WHERE usr = ?1 AND generation = ")
        + k_active_generation + " LIMIT 1) ORDER BY qualified_name;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_text(statement, 1, usr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SSymbol symbol;
        symbol.m_usr = read_text(statement, 0);
        symbol.m_kind = symbol_kind_from_name(read_text(statement, 1));
        symbol.m_qualified_name = read_text(statement, 2);
        symbol.m_name = read_text(statement, 3);
        symbol.m_signature = read_text(statement, 4);
        symbol.m_access = read_text(statement, 5);
        symbol.m_is_definition = (sqlite3_column_int(statement, 6) != 0);
        const int flags = sqlite3_column_int(statement, 7);
        symbol.m_is_virtual = ((flags & 1) != 0);
        symbol.m_is_pure_virtual = ((flags & 2) != 0);
        symbol.m_is_static = ((flags & 4) != 0);
        symbol.m_location.m_file = read_text(statement, 8);
        result.push_back(std::move(symbol));
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<SSymbol> CSQLiteIndex::callers(const std::string& usr) const
{
    std::vector<SSymbol> result;
    if (m_db == nullptr || usr.empty())
    {
        return result;
    }

    const std::string sql = std::string(
        "SELECT s.usr, s.kind, s.qualified_name, s.name, s.signature, s.access, s.is_definition, s.flags, "
        "COALESCE((SELECT path FROM file WHERE id = s.file_id), '') "
        "FROM call c JOIN symbol s ON s.id = c.caller_id "
        "WHERE c.callee_id = (SELECT id FROM symbol WHERE usr = ?1 AND generation = ")
        + k_active_generation + " LIMIT 1) GROUP BY s.id ORDER BY s.qualified_name;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_text(statement, 1, usr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SSymbol symbol;
        symbol.m_usr = read_text(statement, 0);
        symbol.m_kind = symbol_kind_from_name(read_text(statement, 1));
        symbol.m_qualified_name = read_text(statement, 2);
        symbol.m_name = read_text(statement, 3);
        symbol.m_signature = read_text(statement, 4);
        symbol.m_location.m_file = read_text(statement, 8);
        result.push_back(std::move(symbol));
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<SSymbol> CSQLiteIndex::callees(const std::string& usr) const
{
    std::vector<SSymbol> result;
    if (m_db == nullptr || usr.empty())
    {
        return result;
    }

    const std::string sql = std::string(
        "SELECT s.usr, s.kind, s.qualified_name, s.name, s.signature, s.access, s.is_definition, s.flags, "
        "COALESCE((SELECT path FROM file WHERE id = s.file_id), '') "
        "FROM call c JOIN symbol s ON s.id = c.callee_id "
        "WHERE c.caller_id = (SELECT id FROM symbol WHERE usr = ?1 AND generation = ")
        + k_active_generation + " LIMIT 1) GROUP BY s.id ORDER BY s.qualified_name;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_text(statement, 1, usr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SSymbol symbol;
        symbol.m_usr = read_text(statement, 0);
        symbol.m_kind = symbol_kind_from_name(read_text(statement, 1));
        symbol.m_qualified_name = read_text(statement, 2);
        symbol.m_name = read_text(statement, 3);
        symbol.m_signature = read_text(statement, 4);
        symbol.m_location.m_file = read_text(statement, 8);
        result.push_back(std::move(symbol));
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<SSymbolLocation> CSQLiteIndex::references(const std::string& usr) const
{
    std::vector<SSymbolLocation> result;
    if (m_db == nullptr || usr.empty())
    {
        return result;
    }

    const std::string sql = std::string(
        "SELECT COALESCE((SELECT path FROM file WHERE id = r.file_id), ''), r.line, r.column "
        "FROM reference r WHERE r.symbol_id = (SELECT id FROM symbol WHERE usr = ?1 AND generation = ")
        + k_active_generation + " LIMIT 1) ORDER BY r.file_id, r.line;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_text(statement, 1, usr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SSymbolLocation location;
        location.m_file = read_text(statement, 0);
        location.m_range.m_line = static_cast<std::uint32_t>(sqlite3_column_int(statement, 1));
        location.m_range.m_column = static_cast<std::uint32_t>(sqlite3_column_int(statement, 2));
        location.m_range.m_end_line = location.m_range.m_line;
        location.m_range.m_end_column = location.m_range.m_column;
        result.push_back(std::move(location));
    }
    sqlite3_finalize(statement);
    return result;
}

std::optional<SSourceSlice> CSQLiteIndex::get_source_slice(const std::string& usr) const
{
    if (m_db == nullptr || usr.empty())
    {
        return std::nullopt;
    }

    const std::string sql = std::string(
        "SELECT sl.line, sl.column, sl.end_line, sl.end_column, "
        "COALESCE((SELECT path FROM file WHERE id = sl.file_id), '') "
        "FROM symbol_location sl WHERE sl.symbol_id = (SELECT id FROM symbol WHERE usr = ?1 AND generation = ")
        + k_active_generation + " LIMIT 1) ORDER BY sl.line, sl.column LIMIT 1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }
    sqlite3_bind_text(statement, 1, usr.c_str(), -1, SQLITE_TRANSIENT);

    std::uint32_t start_line = 0;
    std::uint32_t start_column = 0;
    std::uint32_t end_line = 0;
    std::uint32_t end_column = 0;
    std::string file;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        start_line = static_cast<std::uint32_t>(sqlite3_column_int(statement, 0));
        start_column = static_cast<std::uint32_t>(sqlite3_column_int(statement, 1));
        end_line = static_cast<std::uint32_t>(sqlite3_column_int(statement, 2));
        end_column = static_cast<std::uint32_t>(sqlite3_column_int(statement, 3));
        file = read_text(statement, 4);
    }
    sqlite3_finalize(statement);

    if (file.empty() || start_line == 0 || end_line < start_line)
    {
        return std::nullopt;
    }

    std::ifstream input(file);
    if (!input.is_open())
    {
        return std::nullopt;
    }

    // libclang ranges are half-open: start column is included, end column is excluded.
    // Older/incomplete index rows may lack columns; preserve the line-based fallback.
    const bool has_exact_columns = start_column > 0 && end_column > 0;
    std::ostringstream slice;
    std::string line;
    std::uint32_t current = 0;
    while (std::getline(input, line))
    {
        ++current;
        if (current < start_line)
        {
            continue;
        }
        if (current > end_line)
        {
            break;
        }

        if (has_exact_columns)
        {
            const std::size_t start_offset =
                (current == start_line) ? std::min<std::size_t>(start_column - 1, line.size()) : 0;
            const std::size_t end_offset =
                (current == end_line) ? std::min<std::size_t>(end_column - 1, line.size()) : line.size();
            if (end_offset >= start_offset)
            {
                slice << line.substr(start_offset, end_offset - start_offset);
            }
        }
        else
        {
            slice << line;
        }
        slice << '\n';
    }

    SSourceSlice result;
    result.m_usr = usr;
    result.m_text = slice.str();
    result.m_range.m_line = start_line;
    result.m_range.m_column = start_column;
    result.m_range.m_end_line = end_line;
    result.m_range.m_end_column = end_column;
    return result;
}

bool CSQLiteIndex::warm(IClang& clang, const std::vector<std::string>& reindex_paths, bool full)
{
    if (m_db == nullptr)
    {
        return false;
    }

    const long long generation = active_generation();
    if (generation == 0)
    {
        return false;
    }

    /**
     * @brief Строка файла generation.
     */
    struct SFileRow
    {
        sqlite3_int64 m_id;      ///< Идентификатор файла.
        std::string m_arguments; ///< Аргументы компиляции (пусто для заголовков).
        bool m_is_tu;            ///< Является ли translation unit.
    };

    std::map<std::string, SFileRow> all_files;
    std::vector<std::string> all_paths;

    {
        sqlite3_stmt* statement = nullptr;
        const char* sql = "SELECT f.id, f.path, c.arguments FROM file f "
                          "LEFT JOIN compile_command c ON c.file_id = f.id WHERE f.generation = ?1;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            const std::string path = read_text(statement, 1);
            SFileRow row;
            row.m_id = sqlite3_column_int64(statement, 0);
            row.m_arguments = read_text(statement, 2);
            row.m_is_tu = (sqlite3_column_type(statement, 2) != SQLITE_NULL);
            all_files[path] = row;
            all_paths.push_back(path);
        }
        sqlite3_finalize(statement);
    }

    // Множество путей для переиндексации: всё (full) либо dirty TU + их заголовки.
    std::set<std::string> reindex_set;
    if (full)
    {
        for (const std::string& path : all_paths)
        {
            reindex_set.insert(path);
        }
    }
    else
    {
        std::vector<std::string> queue(reindex_paths.begin(), reindex_paths.end());
        std::set<std::string> visited;
        while (!queue.empty())
        {
            const std::string current = queue.back();
            queue.pop_back();
            if (!visited.insert(current).second)
            {
                continue;
            }

            const auto file_iterator = all_files.find(current);
            if (file_iterator == all_files.end())
            {
                continue;
            }
            reindex_set.insert(current);

            sqlite3_stmt* statement = nullptr;
            const char* sql = "SELECT to_path FROM include WHERE from_file_id = ?1 AND generation = ?2;";
            if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
            {
                continue;
            }
            sqlite3_bind_int64(statement, 1, file_iterator->second.m_id);
            sqlite3_bind_int64(statement, 2, generation);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                const std::string target = read_text(statement, 0);
                if (all_files.find(target) != all_files.end())
                {
                    queue.push_back(target);
                }
            }
            sqlite3_finalize(statement);
        }
    }

    // Точечное обновление временно отключает FK: строки переиндексируемых файлов
    // удаляются, связи переустанавливаются по USR после вставки.
    if (!full)
    {
        exec_sql("PRAGMA foreign_keys=OFF;");
    }

    if (!exec_sql("BEGIN IMMEDIATE;"))
    {
        return false;
    }

    // Точечное обновление: удалить данные только переиндексируемых файлов.
    if (!full)
    {
        const char* queries[] =
        {
            "DELETE FROM symbol_location WHERE symbol_id IN (SELECT id FROM symbol WHERE file_id = ?1);",
            "DELETE FROM reference WHERE file_id = ?1 OR symbol_id IN (SELECT id FROM symbol WHERE file_id = ?1);",
            "DELETE FROM call WHERE file_id = ?1 OR caller_id IN (SELECT id FROM symbol WHERE file_id = ?1) OR callee_id IN (SELECT id FROM symbol WHERE file_id = ?1);",
            "DELETE FROM override WHERE method_id IN (SELECT id FROM symbol WHERE file_id = ?1) OR base_method_id IN (SELECT id FROM symbol WHERE file_id = ?1);",
            "DELETE FROM inheritance WHERE derived_id IN (SELECT id FROM symbol WHERE file_id = ?1) OR base_id IN (SELECT id FROM symbol WHERE file_id = ?1);",
            "DELETE FROM include WHERE from_file_id = ?1;",
            "DELETE FROM symbol WHERE file_id = ?1;"
        };

        for (const std::string& path : reindex_set)
        {
            const auto file_iterator = all_files.find(path);
            if (file_iterator == all_files.end())
            {
                continue;
            }
            const sqlite3_int64 file_id = file_iterator->second.m_id;

            for (const char* query : queries)
            {
                sqlite3_stmt* statement = nullptr;
                if (sqlite3_prepare_v2(m_db, query, -1, &statement, nullptr) != SQLITE_OK)
                {
                    exec_sql("ROLLBACK;");
                    return false;
                }
                sqlite3_bind_int64(statement, 1, file_id);
                const bool done = (sqlite3_step(statement) == SQLITE_DONE);
                sqlite3_finalize(statement);
                if (!done)
                {
                    exec_sql("ROLLBACK;");
                    if (!full)
                    {
                        exec_sql("PRAGMA foreign_keys=ON;");
                    }
                    return false;
                }
            }
        }
    }

    std::map<std::string, sqlite3_int64> path_to_file;
    for (const auto& entry : all_files)
    {
        path_to_file[entry.first] = entry.second.m_id;
    }

    // Предзагрузка существующих USR (для точечного обновления и линковки связей).
    std::unordered_map<std::string, sqlite3_int64> symbol_ids;
    {
        sqlite3_stmt* statement = nullptr;
        const char* sql = "SELECT usr, id FROM symbol WHERE generation = ?1;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(statement, 1, generation);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                symbol_ids[read_text(statement, 0)] = sqlite3_column_int64(statement, 1);
            }
            sqlite3_finalize(statement);
        }
    }

    std::vector<STuJob> jobs;
    for (const std::string& path : all_paths)
    {
        const auto file_iterator = all_files.find(path);
        if (file_iterator == all_files.end() || !file_iterator->second.m_is_tu)
        {
            continue;
        }
        if (reindex_set.find(path) == reindex_set.end())
        {
            continue;
        }

        STuJob job;
        job.m_file_id = file_iterator->second.m_id;
        job.m_path = path;
        job.m_arguments = split_arguments(file_iterator->second.m_arguments);
        jobs.push_back(std::move(job));
    }

    // --- Стейджинг: каждый TU парсится ровно один раз. ---
    std::map<std::string, SStagedSymbol> staged_symbols;
    std::vector<SStagedBase> staged_bases;
    std::vector<SStagedOverride> staged_overrides;
    std::vector<SStagedReference> staged_references;
    std::vector<SStagedCall> staged_calls;
    std::vector<SStagedInclude> staged_includes;
    std::set<std::string> base_seen;
    std::set<std::string> override_seen;
    std::set<std::string> include_seen;
    std::set<std::string> reference_seen;
    std::set<std::string> call_seen;

    for (const STuJob& job : jobs)
    {
        SClangExtract extract_result;
        if (!clang.extract(job.m_path, job.m_arguments, extract_result))
        {
            continue;
        }

        for (const SClangSymbol& symbol : extract_result.m_symbols)
        {
            if (symbol.m_usr.empty())
            {
                continue;
            }

            // Не индексируем внешние/системные символы (заголовки stdlib, _deps).
            if (is_external_path(symbol.m_location.m_file, m_project_root.string()))
            {
                continue;
            }

            const auto existing = staged_symbols.find(symbol.m_usr);
            const bool has_existing = (existing != staged_symbols.end());
            if (has_existing && !(symbol.m_is_definition && !existing->second.m_is_definition))
            {
                continue;
            }

            SStagedSymbol staged;
            staged.m_usr = symbol.m_usr;
            staged.m_kind = symbol.m_kind;
            staged.m_name = symbol.m_name;
            staged.m_qualified_name = symbol.m_qualified_name;
            staged.m_signature = symbol.m_signature;
            staged.m_access = symbol.m_access;
            staged.m_is_definition = symbol.m_is_definition;
            staged.m_is_virtual = symbol.m_is_virtual;
            staged.m_is_pure_virtual = symbol.m_is_pure_virtual;
            staged.m_is_static = symbol.m_is_static;
            staged.m_parent_usr = symbol.m_parent_usr;
            staged.m_file = symbol.m_location.m_file;
            staged.m_line = symbol.m_location.m_line;
            staged.m_column = symbol.m_location.m_column;
            staged.m_end_line = symbol.m_location.m_end_line;
            staged.m_end_column = symbol.m_location.m_end_column;
            staged_symbols[symbol.m_usr] = std::move(staged);
        }

        for (const SClangBase& base : extract_result.m_bases)
        {
            const std::string key = base.m_derived_usr + "|" + base.m_base_usr;
            if (base_seen.insert(key).second)
            {
                staged_bases.push_back(SStagedBase{base.m_derived_usr, base.m_base_usr, base.m_base_qname,
                                                   base.m_access, base.m_is_virtual});
            }
        }

        for (const SClangSymbol& symbol : extract_result.m_symbols)
        {
            if (symbol.m_overridden_usr.empty())
            {
                continue;
            }
            const std::string key = symbol.m_usr + "|" + symbol.m_overridden_usr;
            if (override_seen.insert(key).second)
            {
                staged_overrides.push_back(SStagedOverride{symbol.m_usr, symbol.m_overridden_usr});
            }
        }

        for (const SClangReference& reference : extract_result.m_references)
        {
            const std::string key = reference.m_symbol_usr + "|" + reference.m_location.m_file
                                    + "|" + std::to_string(reference.m_location.m_line)
                                    + "|" + std::to_string(reference.m_location.m_column);
            if (reference_seen.insert(key).second)
            {
                staged_references.push_back(SStagedReference{reference.m_symbol_usr, reference.m_kind,
                                                             reference.m_location.m_file,
                                                             reference.m_location.m_line,
                                                             reference.m_location.m_column});
            }
        }

        for (const SClangCall& call : extract_result.m_calls)
        {
            const std::string key = call.m_caller_usr + "|" + call.m_callee_usr
                                    + "|" + call.m_location.m_file
                                    + "|" + std::to_string(call.m_location.m_line);
            if (call_seen.insert(key).second)
            {
                staged_calls.push_back(SStagedCall{call.m_caller_usr, call.m_callee_usr, call.m_callee_qname,
                                                   call.m_location.m_file, call.m_location.m_line,
                                                   call.m_location.m_column});
            }
        }

        for (const SClangInclude& include : extract_result.m_includes)
        {
            if (!m_project_root.empty()
                    && !include.m_to_path.starts_with(m_project_root.string()))
            {
                continue;
            }

            const std::string key = include.m_from_file + "|" + include.m_to_path;
            if (include_seen.insert(key).second)
            {
                staged_includes.push_back(SStagedInclude{include.m_from_file, include.m_to_path, include.m_is_angle});
            }
        }
    }

    // --- Новые файлы определяются только при полном построении. ---
    if (full)
    {
        std::set<std::string> extra_files;
        for (const auto& entry : staged_symbols)
        {
            if (!entry.second.m_file.empty() && path_to_file.find(entry.second.m_file) == path_to_file.end())
            {
                extra_files.insert(entry.second.m_file);
            }
        }
        for (const SStagedInclude& include : staged_includes)
        {
            if (is_external_path(include.m_to_path, m_project_root.string()))
            {
                continue;
            }
            if (!include.m_to_path.empty() && path_to_file.find(include.m_to_path) == path_to_file.end())
            {
                extra_files.insert(include.m_to_path);
            }
        }

        for (const std::string& path : extra_files)
        {
            std::error_code error_code;
            const std::uint64_t size = std::filesystem::file_size(path, error_code);
            const std::uint64_t resolved_size = error_code ? 0u : size;
            sqlite3_int64 mtime = 0;
            if (!error_code)
            {
                const auto file_time = std::filesystem::last_write_time(path, error_code);
                if (!error_code)
                {
                    mtime = std::chrono::duration_cast<std::chrono::seconds>(file_time.time_since_epoch()).count();
                }
            }

            sqlite3_stmt* statement = nullptr;
            if (sqlite3_prepare_v2(m_db, "INSERT INTO file (path, generation, mtime, size) VALUES (?1, ?2, ?3, ?4);",
                                   -1, &statement, nullptr) != SQLITE_OK)
            {
                exec_sql("ROLLBACK;");
                return false;
            }
            sqlite3_bind_text(statement, 1, path.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(statement, 2, generation);
            sqlite3_bind_int64(statement, 3, mtime);
            sqlite3_bind_int64(statement, 4, static_cast<sqlite3_int64>(resolved_size));
            const bool inserted = (sqlite3_step(statement) == SQLITE_DONE);
            sqlite3_finalize(statement);
            if (!inserted)
            {
                exec_sql("ROLLBACK;");
                return false;
            }
            path_to_file[path] = sqlite3_last_insert_rowid(m_db);
        }
    }

    std::vector<std::pair<sqlite3_int64, std::string>> parent_links;
    std::size_t symbol_total = 0;

    for (const auto& entry : staged_symbols)
    {
        const SStagedSymbol& symbol = entry.second;

        sqlite3_int64 symbol_file_id = 0;
        const auto file_iterator = path_to_file.find(symbol.m_file);
        if (file_iterator != path_to_file.end())
        {
            symbol_file_id = file_iterator->second;
        }

        const int flags = (symbol.m_is_virtual ? 1 : 0)
                          | (symbol.m_is_pure_virtual ? 2 : 0)
                          | (symbol.m_is_static ? 4 : 0);
        const std::string kind_name = m_symbol_kind_names[static_cast<std::size_t>(symbol.m_kind)];

        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "INSERT INTO symbol (generation, usr, kind, name, qualified_name, file_id, parent_id, "
            "is_definition, is_declaration, signature, access, flags, parent_usr) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, NULL, ?7, 1, ?8, ?9, ?10, ?11);";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        sqlite3_bind_text(statement, 2, symbol.m_usr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, kind_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, symbol.m_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, symbol.m_qualified_name.c_str(), -1, SQLITE_TRANSIENT);
        if (symbol_file_id > 0)
        {
            sqlite3_bind_int64(statement, 6, symbol_file_id);
        }
        else
        {
            sqlite3_bind_null(statement, 6);
        }
        sqlite3_bind_int(statement, 7, symbol.m_is_definition ? 1 : 0);
        sqlite3_bind_text(statement, 8, symbol.m_signature.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 9, symbol.m_access.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 10, flags);
        sqlite3_bind_text(statement, 11, symbol.m_parent_usr.c_str(), -1, SQLITE_TRANSIENT);
        const bool inserted = (sqlite3_step(statement) == SQLITE_DONE);
        sqlite3_finalize(statement);
        if (!inserted)
        {
            exec_sql("ROLLBACK;");
            return false;
        }

        const sqlite3_int64 symbol_id = sqlite3_last_insert_rowid(m_db);
        symbol_ids[symbol.m_usr] = symbol_id;
        ++symbol_total;

        if (!symbol.m_parent_usr.empty())
        {
            parent_links.emplace_back(symbol_id, symbol.m_parent_usr);
        }

        sqlite3_stmt* location_statement = nullptr;
        const char* location_sql =
            "INSERT INTO symbol_location (symbol_id, file_id, line, column, end_line, end_column) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6);";
        if (sqlite3_prepare_v2(m_db, location_sql, -1, &location_statement, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(location_statement, 1, symbol_id);
            if (symbol_file_id > 0)
            {
                sqlite3_bind_int64(location_statement, 2, symbol_file_id);
            }
            else
            {
                sqlite3_bind_null(location_statement, 2);
            }
            sqlite3_bind_int(location_statement, 3, static_cast<int>(symbol.m_line));
            sqlite3_bind_int(location_statement, 4, static_cast<int>(symbol.m_column));
            sqlite3_bind_int(location_statement, 5, static_cast<int>(symbol.m_end_line));
            sqlite3_bind_int(location_statement, 6, static_cast<int>(symbol.m_end_column));
            sqlite3_step(location_statement);
            sqlite3_finalize(location_statement);
        }
    }

    for (const std::pair<sqlite3_int64, std::string>& link : parent_links)
    {
        const auto parent_iterator = symbol_ids.find(link.second);
        if (parent_iterator == symbol_ids.end())
        {
            continue;
        }

        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db, "UPDATE symbol SET parent_id = ?1 WHERE id = ?2;", -1, &statement, nullptr) != SQLITE_OK)
        {
            continue;
        }
        sqlite3_bind_int64(statement, 1, parent_iterator->second);
        sqlite3_bind_int64(statement, 2, link.first);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

    for (const SStagedBase& base : staged_bases)
    {
        const auto derived_iterator = symbol_ids.find(base.m_derived_usr);
        if (derived_iterator == symbol_ids.end())
        {
            continue;
        }

        sqlite3_int64 base_id = 0;
        const auto base_iterator = symbol_ids.find(base.m_base_usr);
        if (base_iterator != symbol_ids.end())
        {
            base_id = base_iterator->second;
        }

        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "INSERT INTO inheritance (generation, derived_id, base_id, base_qname, access, is_virtual) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6);";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        sqlite3_bind_int64(statement, 2, derived_iterator->second);
        if (base_id > 0)
        {
            sqlite3_bind_int64(statement, 3, base_id);
        }
        else
        {
            sqlite3_bind_null(statement, 3);
        }
        sqlite3_bind_text(statement, 4, base.m_base_qname.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, base.m_access.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 6, base.m_is_virtual ? 1 : 0);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

    for (const SStagedOverride& entry : staged_overrides)
    {
        const auto method_iterator = symbol_ids.find(entry.m_method_usr);
        const auto base_iterator = symbol_ids.find(entry.m_base_method_usr);
        if (method_iterator == symbol_ids.end() || base_iterator == symbol_ids.end())
        {
            continue;
        }

        sqlite3_stmt* statement = nullptr;
        const char* sql = "INSERT INTO override (generation, method_id, base_method_id) VALUES (?1, ?2, ?3);";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        sqlite3_bind_int64(statement, 2, method_iterator->second);
        sqlite3_bind_int64(statement, 3, base_iterator->second);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

    for (const SStagedReference& reference : staged_references)
    {
        const auto symbol_iterator = symbol_ids.find(reference.m_symbol_usr);
        if (symbol_iterator == symbol_ids.end())
        {
            continue;
        }

        sqlite3_int64 file_id = 0;
        const auto file_iterator = path_to_file.find(reference.m_file);
        if (file_iterator != path_to_file.end())
        {
            file_id = file_iterator->second;
        }
        if (file_id == 0)
        {
            continue;
        }

        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "INSERT INTO reference (generation, symbol_id, file_id, line, column, kind) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6);";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        sqlite3_bind_int64(statement, 2, symbol_iterator->second);
        sqlite3_bind_int64(statement, 3, file_id);
        sqlite3_bind_int(statement, 4, static_cast<int>(reference.m_line));
        sqlite3_bind_int(statement, 5, static_cast<int>(reference.m_column));
        sqlite3_bind_text(statement, 6, reference.m_kind.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

    for (const SStagedCall& call : staged_calls)
    {
        const auto caller_iterator = symbol_ids.find(call.m_caller_usr);
        if (caller_iterator == symbol_ids.end())
        {
            continue;
        }

        sqlite3_int64 callee_id = 0;
        const auto callee_iterator = symbol_ids.find(call.m_callee_usr);
        if (callee_iterator != symbol_ids.end())
        {
            callee_id = callee_iterator->second;
        }

        sqlite3_int64 file_id = 0;
        const auto file_iterator = path_to_file.find(call.m_file);
        if (file_iterator != path_to_file.end())
        {
            file_id = file_iterator->second;
        }

        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "INSERT INTO call (generation, caller_id, callee_id, callee_qname, file_id, line, column) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        sqlite3_bind_int64(statement, 2, caller_iterator->second);
        if (callee_id > 0)
        {
            sqlite3_bind_int64(statement, 3, callee_id);
        }
        else
        {
            sqlite3_bind_null(statement, 3);
        }
        sqlite3_bind_text(statement, 4, call.m_callee_qname.c_str(), -1, SQLITE_TRANSIENT);
        if (file_id > 0)
        {
            sqlite3_bind_int64(statement, 5, file_id);
        }
        else
        {
            sqlite3_bind_null(statement, 5);
        }
        sqlite3_bind_int(statement, 6, static_cast<int>(call.m_line));
        sqlite3_bind_int(statement, 7, static_cast<int>(call.m_column));
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

    for (const SStagedInclude& include : staged_includes)
    {
        sqlite3_int64 from_file_id = 0;
        const auto from_iterator = path_to_file.find(include.m_from_file);
        if (from_iterator != path_to_file.end())
        {
            from_file_id = from_iterator->second;
        }

        sqlite3_int64 resolved_id = 0;
        const auto resolved_iterator = path_to_file.find(include.m_to_path);
        if (resolved_iterator != path_to_file.end())
        {
            resolved_id = resolved_iterator->second;
        }

        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "INSERT INTO include (generation, from_file_id, to_path, resolved_file_id, is_angle) "
            "VALUES (?1, ?2, ?3, ?4, ?5);";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            exec_sql("ROLLBACK;");
            return false;
        }
        sqlite3_bind_int64(statement, 1, generation);
        sqlite3_bind_int64(statement, 2, from_file_id);
        sqlite3_bind_text(statement, 3, include.m_to_path.c_str(), -1, SQLITE_TRANSIENT);
        if (resolved_id > 0)
        {
            sqlite3_bind_int64(statement, 4, resolved_id);
        }
        else
        {
            sqlite3_bind_null(statement, 4);
        }
        sqlite3_bind_int(statement, 5, include.m_is_angle ? 1 : 0);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

    {
        sqlite3_stmt* statement = nullptr;
        const char* sql = "UPDATE index_generation SET symbol_count = ?1 WHERE generation = ?2;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(symbol_total));
            sqlite3_bind_int64(statement, 2, generation);
            sqlite3_step(statement);
            sqlite3_finalize(statement);
        }
    }

    if (!exec_sql("COMMIT;"))
    {
        exec_sql("ROLLBACK;");
        if (!full)
        {
            exec_sql("PRAGMA foreign_keys=ON;");
        }
        return false;
    }

    if (!full)
    {
        relink_parent_ids(generation);
        exec_sql("PRAGMA foreign_keys=ON;");
    }

    compute_content_hashes();
    return true;
}

void CSQLiteIndex::set_compile_commands_fingerprint(const std::string& compile_commands_path)
{
    if (m_db == nullptr || compile_commands_path.empty())
    {
        return;
    }

    std::string hash;
    if (!compute_file_hash(compile_commands_path, hash))
    {
        return;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db,
                           "INSERT INTO schema_meta (key, value) VALUES ('compile_commands_hash', ?1) "
                           "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
                           -1, &statement, nullptr) != SQLITE_OK)
    {
        return;
    }
    sqlite3_bind_text(statement, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
}

void CSQLiteIndex::compute_content_hashes() const
{
    if (m_db == nullptr)
    {
        return;
    }

    const long long generation = active_generation();
    if (generation == 0)
    {
        return;
    }

    for (const std::string& path : file_paths())
    {
        std::string hash;
        if (!compute_file_hash(path, hash))
        {
            continue;
        }

        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db, "UPDATE file SET hash = ?1 WHERE path = ?2 AND generation = ?3;",
                               -1, &statement, nullptr) != SQLITE_OK)
        {
            continue;
        }
        sqlite3_bind_text(statement, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement, 3, generation);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }
}

std::vector<std::string> CSQLiteIndex::file_paths() const
{
    std::vector<std::string> paths;
    if (m_db == nullptr)
    {
        return paths;
    }

    const std::string sql = std::string("SELECT path FROM file WHERE generation = ") + k_active_generation + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return paths;
    }
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        paths.push_back(read_text(statement, 0));
    }
    sqlite3_finalize(statement);
    return paths;
}

bool CSQLiteIndex::detect_stale(const std::string& external_compile_commands, SStaleReport& out_report) const
{
    out_report = SStaleReport{};

    if (m_db == nullptr)
    {
        out_report.m_reason = EStaleReason::Missing;
        return false;
    }

    const long long generation = active_generation();
    if (generation == 0)
    {
        out_report.m_reason = EStaleReason::Missing;
        return true;
    }

    out_report.m_valid = true;

    // 1. Инвалидация окружения: изменился compile_commands.json.
    if (!external_compile_commands.empty())
    {
        const std::string stored = read_schema_meta(m_db, "compile_commands_hash");
        std::string current;
        if (compute_file_hash(external_compile_commands, current) && !stored.empty() && stored != current)
        {
            out_report.m_reason = EStaleReason::EnvironmentChange;
            return true;
        }
    }

    // 2-3. Изменение содержимого файлов (mtime+size, затем подтверждение хэшем).
    struct SFileRecord
    {
        std::string m_path;
        std::int64_t m_mtime;
        std::uint64_t m_size;
        std::string m_hash;
    };

    std::vector<SFileRecord> files;
    {
        const std::string sql = std::string(
            "SELECT path, COALESCE(mtime, 0), COALESCE(size, 0), COALESCE(hash, '') "
            "FROM file WHERE generation = ") + k_active_generation + ";";

        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
        {
            return false;
        }
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            SFileRecord record;
            record.m_path = read_text(statement, 0);
            record.m_mtime = sqlite3_column_int64(statement, 1);
            record.m_size = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 2));
            record.m_hash = read_text(statement, 3);
            files.push_back(std::move(record));
        }
        sqlite3_finalize(statement);
    }

    std::set<std::string> dirty;
    for (const SFileRecord& record : files)
    {
        std::error_code error_code;
        if (!std::filesystem::exists(record.m_path, error_code) || error_code)
        {
            out_report.m_removed_files.push_back(record.m_path);
            // Удалённый файл, возможно, всё ещё включён в активный TU.
            dirty.insert(record.m_path);
            continue;
        }

        const std::int64_t mtime = file_mtime(record.m_path);
        const std::uint64_t size = file_size(record.m_path);
        bool changed = (mtime != record.m_mtime) || (size != record.m_size);

        if (changed && !record.m_hash.empty())
        {
            std::string current;
            if (compute_file_hash(record.m_path, current) && current == record.m_hash)
            {
                changed = false;
            }
        }

        if (changed)
        {
            const std::string extension = std::filesystem::path(record.m_path).extension().string();
            const bool is_source = (extension == ".cpp" || extension == ".cc" || extension == ".cxx"
                                    || extension == ".c" || extension == ".m" || extension == ".mm");
            if (is_source)
            {
                dirty.insert(record.m_path);
            }
            else
            {
                // Изменённый заголовок: все включающие его TU.
                include_dependents(record.m_path, dirty);
            }
        }
    }

    // 4. Новые файлы (в этом инкременте не поддерживаются — требует нового инвентаря).
    const std::vector<std::string> externals = external_tu_paths(external_compile_commands);
    for (const std::string& path : externals)
    {
        bool known = false;
        for (const SFileRecord& record : files)
        {
            if (record.m_path == path)
            {
                known = true;
                break;
            }
        }
        if (!known)
        {
            out_report.m_reason = EStaleReason::EnvironmentChange;
            return true;
        }
    }

    if (!dirty.empty() || !out_report.m_removed_files.empty())
    {
        out_report.m_reason = EStaleReason::ContentChange;
        out_report.m_dirty_tus.assign(dirty.begin(), dirty.end());
    }

    return true;
}

void CSQLiteIndex::include_dependents(const std::string& header_path, std::set<std::string>& out_tus) const
{
    if (m_db == nullptr)
    {
        return;
    }

    std::vector<std::string> queue{header_path};
    std::set<std::string> visited;

    while (!queue.empty())
    {
        const std::string current = queue.back();
        queue.pop_back();
        if (!visited.insert(current).second)
        {
            continue;
        }

        sqlite3_stmt* statement = nullptr;
        const std::string sql = std::string(
            "SELECT DISTINCT f.path FROM include i JOIN file f ON f.id = i.from_file_id "
            "WHERE i.to_path = ?1 AND i.generation = ") + k_active_generation + ";";
        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
        {
            continue;
        }
        sqlite3_bind_text(statement, 1, current.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            const std::string includer = read_text(statement, 0);
            const std::string extension = std::filesystem::path(includer).extension().string();
            const bool is_source = (extension == ".cpp" || extension == ".cc" || extension == ".cxx"
                                    || extension == ".c" || extension == ".m" || extension == ".mm");
            if (is_source)
            {
                out_tus.insert(includer);
            }
            else
            {
                queue.push_back(includer);
            }
        }
        sqlite3_finalize(statement);
    }
}

std::vector<std::string> CSQLiteIndex::external_tu_paths(const std::string& external_compile_commands) const
{
    std::vector<std::string> paths;
    if (external_compile_commands.empty())
    {
        return paths;
    }

    std::ifstream input(external_compile_commands);
    if (!input.is_open())
    {
        return paths;
    }

    // Минимальный парсер: извлекаем значения "file" без зависимости от json в этом TU.
    const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::string key = "\"file\"";
    std::size_t position = 0;
    while ((position = content.find(key, position)) != std::string::npos)
    {
        const std::size_t colon = content.find(':', position);
        const std::size_t first_quote = (colon != std::string::npos) ? content.find('"', colon) : std::string::npos;
        const std::size_t second_quote = (first_quote != std::string::npos) ? content.find('"', first_quote + 1) : std::string::npos;
        if (first_quote == std::string::npos || second_quote == std::string::npos)
        {
            break;
        }
        paths.push_back(content.substr(first_quote + 1, second_quote - first_quote - 1));
        position = second_quote + 1;
    }

    return paths;
}

bool CSQLiteIndex::refresh_if_stale(IClang& clang, const std::string& external_compile_commands)
{
    SStaleReport report;
    if (!detect_stale(external_compile_commands, report))
    {
        return false;
    }

    if (!report.is_stale())
    {
        return true;
    }

    if (report.m_reason == EStaleReason::EnvironmentChange || report.m_reason == EStaleReason::Missing)
    {
        std::vector<SCompileCommand> commands;
        if (!load_current_commands(commands))
        {
            return false;
        }
        return rebuild(clang, commands);
    }

    // ContentChange: точечная переиндексация изменённых TU и их заголовков.
    if (!warm(clang, report.m_dirty_tus, false))
    {
        return false;
    }

    clear_removed_files(report.m_removed_files);
    compute_content_hashes();
    return true;
}

void CSQLiteIndex::relink_parent_ids(long long generation)
{
    if (m_db == nullptr)
    {
        return;
    }

    exec_sql("UPDATE symbol SET parent_id = (SELECT p.id FROM symbol p "
             "WHERE p.generation = (SELECT generation FROM index_generation WHERE state = 'ACTIVE' "
             "ORDER BY generation DESC LIMIT 1) AND p.usr = symbol.parent_usr LIMIT 1) "
             "WHERE parent_usr IS NOT NULL AND parent_usr <> '';");

    exec_sql("UPDATE inheritance SET base_id = (SELECT b.id FROM symbol b "
             "WHERE b.generation = (SELECT generation FROM index_generation WHERE state = 'ACTIVE' "
             "ORDER BY generation DESC LIMIT 1) AND b.usr = inheritance.base_qname LIMIT 1) "
             "WHERE base_id IS NULL;");

    (void)generation;
}

void CSQLiteIndex::clear_removed_files(const std::vector<std::string>& removed_files)
{
    for (const std::string& path : removed_files)
    {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db,
                               "UPDATE file SET mtime = 0, size = 0, hash = '' WHERE path = ?1 AND generation = "
                               "(SELECT generation FROM index_generation WHERE state = 'ACTIVE' ORDER BY generation DESC LIMIT 1);",
                               -1, &statement, nullptr) != SQLITE_OK)
        {
            continue;
        }
        sqlite3_bind_text(statement, 1, path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }
}

} // namespace fcpp
