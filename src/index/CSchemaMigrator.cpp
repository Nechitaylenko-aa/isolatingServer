/**
 * @file CSchemaMigrator.cpp
 * @brief Реализация движка миграций схемы SQLite (ADR-02).
 */

#include "index/CSchemaMigrator.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <sqlite3.h>

#ifndef FCPP_AGENT_MIGRATIONS_DIR
#define FCPP_AGENT_MIGRATIONS_DIR "migrations"
#endif

namespace fcpp
{

namespace
{

/** @brief Ключ версии схемы в таблице schema_meta. */
constexpr const char* k_schema_version_key = "schema_version";

/** @brief DDL таблицы метаданных схемы. */
constexpr const char* k_schema_meta_ddl =
    "CREATE TABLE IF NOT EXISTS schema_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);";

/**
 * @brief Определить каталог миграций (env-override либо каталог сборки).
 * @return Путь к каталогу с .sql-миграциями.
 */
std::filesystem::path migrations_directory()
{
    const char* override_dir = std::getenv("FCPP_AGENT_MIGRATIONS");
    if (override_dir != nullptr && override_dir[0] != '\0')
    {
        return std::filesystem::path(override_dir);
    }
    return std::filesystem::path(FCPP_AGENT_MIGRATIONS_DIR);
}

} // namespace

bool CSchemaMigrator::exec_sql(sqlite3* db, const char* sql)
{
    if (db == nullptr || sql == nullptr)
    {
        return false;
    }

    char* error_message = nullptr;
    const int result_code = sqlite3_exec(db, sql, nullptr, nullptr, &error_message);
    if (error_message != nullptr)
    {
        sqlite3_free(error_message);
    }

    return result_code == SQLITE_OK;
}

bool CSchemaMigrator::read_schema_version(sqlite3* db, std::uint32_t& out_version)
{
    const std::string query =
        std::string("SELECT value FROM schema_meta WHERE key='") + k_schema_version_key + "';";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bool success = true;
    const int step_result = sqlite3_step(statement);
    if (step_result == SQLITE_ROW)
    {
        const unsigned char* text = sqlite3_column_text(statement, 0);
        if (text != nullptr)
        {
            try
            {
                out_version = static_cast<std::uint32_t>(std::stoul(reinterpret_cast<const char*>(text)));
            }
            catch (const std::exception&)
            {
                success = false;
            }
        }
    }
    else if (step_result != SQLITE_DONE)
    {
        success = false;
    }

    sqlite3_finalize(statement);
    return success;
}

std::uint32_t CSchemaMigrator::parse_version(const std::filesystem::path& path)
{
    const std::string name = path.filename().string();
    std::uint32_t version = 0;
    std::size_t index = 0;
    while (index < name.size() && std::isdigit(static_cast<unsigned char>(name[index])) != 0)
    {
        version = (version * 10) + static_cast<std::uint32_t>(name[index] - '0');
        ++index;
    }
    return version;
}

bool CSchemaMigrator::read_file(const std::filesystem::path& path, std::string& out_content)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    out_content = buffer.str();
    return true;
}

std::vector<std::pair<std::uint32_t, std::filesystem::path>> CSchemaMigrator::collect_migrations()
{
    std::vector<std::pair<std::uint32_t, std::filesystem::path>> entries;

    const std::filesystem::path directory = migrations_directory();
    std::error_code error_code;
    if (!std::filesystem::exists(directory, error_code))
    {
        return entries;
    }

    for (const std::filesystem::directory_entry& item
         : std::filesystem::directory_iterator(directory, error_code))
    {
        if (error_code)
        {
            break;
        }
        if (!item.is_regular_file())
        {
            continue;
        }

        const std::filesystem::path path = item.path();
        if (path.extension() != ".sql")
        {
            continue;
        }

        const std::uint32_t version = parse_version(path);
        if (version == 0)
        {
            continue;
        }

        entries.emplace_back(version, path);
    }

    std::sort(entries.begin(), entries.end(),
              [](const std::pair<std::uint32_t, std::filesystem::path>& left,
                 const std::pair<std::uint32_t, std::filesystem::path>& right)
              {
                  return left.first < right.first;
              });

    return entries;
}

bool CSchemaMigrator::migrate(const std::string& db_path) const
{
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK)
    {
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
        return false;
    }

    bool success = exec_sql(db, "PRAGMA journal_mode=WAL;");
    success = success && exec_sql(db, "PRAGMA foreign_keys=ON;");
    success = success && exec_sql(db, k_schema_meta_ddl);
    if (!success)
    {
        sqlite3_close(db);
        return false;
    }

    std::uint32_t version = 0;
    if (!read_schema_version(db, version))
    {
        sqlite3_close(db);
        return false;
    }

    const std::vector<std::pair<std::uint32_t, std::filesystem::path>> migrations = collect_migrations();
    for (const std::pair<std::uint32_t, std::filesystem::path>& migration : migrations)
    {
        if (migration.first <= version)
        {
            continue;
        }

        std::string sql;
        if (!read_file(migration.second, sql))
        {
            sqlite3_close(db);
            return false;
        }

        if (!exec_sql(db, "BEGIN IMMEDIATE;"))
        {
            sqlite3_close(db);
            return false;
        }

        if (!exec_sql(db, sql.c_str()))
        {
            exec_sql(db, "ROLLBACK;");
            sqlite3_close(db);
            return false;
        }

        const std::string update =
            std::string("INSERT INTO schema_meta (key, value) VALUES ('") + k_schema_version_key
            + "', '" + std::to_string(migration.first)
            + "') ON CONFLICT(key) DO UPDATE SET value = excluded.value;";

        if (!exec_sql(db, update.c_str()))
        {
            exec_sql(db, "ROLLBACK;");
            sqlite3_close(db);
            return false;
        }

        if (!exec_sql(db, "COMMIT;"))
        {
            exec_sql(db, "ROLLBACK;");
            sqlite3_close(db);
            return false;
        }

        version = migration.first;
    }

    sqlite3_close(db);
    return true;
}

std::uint32_t CSchemaMigrator::schema_version(const std::string& db_path) const
{
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK)
    {
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
        return 0;
    }

    std::uint32_t version = 0;
    if (exec_sql(db, k_schema_meta_ddl))
    {
        read_schema_version(db, version);
    }

    sqlite3_close(db);
    return version;
}

} // namespace fcpp
