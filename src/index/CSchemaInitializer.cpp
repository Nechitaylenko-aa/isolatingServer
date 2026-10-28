/**
 * @file CSchemaInitializer.cpp
 * @brief Создание актуальной схемы SQLite и проверка существующих баз.
 */

#include "index/CSchemaInitializer.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_set>
#include <system_error>

#include <sqlite3.h>

#ifndef FCPP_AGENT_SCHEMA_FILE
#define FCPP_AGENT_SCHEMA_FILE "schema/current.sql"
#endif

namespace fcpp
{

namespace
{

constexpr std::array<const char*, 13> k_required_tables = {
    "registry_project",
    "project",
    "index_generation",
    "file",
    "symbol",
    "symbol_location",
    "compile_command",
    "include",
    "inheritance",
    "override",
    "reference",
    "call",
    "project_profile"
};

bool validate_existing_schema(sqlite3* db)
{
    sqlite3_stmt* statement = nullptr;
    constexpr const char* sql =
        "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%';";
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }

    std::unordered_set<std::string> found;
    int result = SQLITE_OK;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW)
    {
        const unsigned char* name = sqlite3_column_text(statement, 0);
        if (name != nullptr)
        {
            found.emplace(reinterpret_cast<const char*>(name));
        }
    }
    const bool query_ok = result == SQLITE_DONE;
    sqlite3_finalize(statement);

    if (!query_ok)
    {
        return false;
    }

    for (const char* table : k_required_tables)
    {
        if (found.find(table) == found.end())
        {
            return false;
        }
    }
    return true;
}

bool execute_sql(sqlite3* db, const std::string& sql)
{
    char* error_message = nullptr;
    const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr)
    {
        sqlite3_free(error_message);
    }
    return result == SQLITE_OK;
}

} // namespace

bool CSchemaInitializer::initialize_or_validate(const std::filesystem::path& db_path)
{
    std::error_code error_code;
    const bool existed_before = std::filesystem::exists(db_path, error_code);
    if (error_code)
    {
        return false;
    }

    if (!db_path.parent_path().empty())
    {
        std::filesystem::create_directories(db_path.parent_path(), error_code);
        if (error_code)
        {
            return false;
        }
    }

    sqlite3* db = nullptr;
    const int flags = SQLITE_OPEN_READWRITE | (existed_before ? 0 : SQLITE_OPEN_CREATE);
    if (sqlite3_open_v2(db_path.string().c_str(), &db, flags, nullptr) != SQLITE_OK)
    {
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
        return false;
    }

    if (existed_before)
    {
        const bool valid = validate_existing_schema(db);
        sqlite3_close(db);
        return valid;
    }

    std::ifstream input(FCPP_AGENT_SCHEMA_FILE, std::ios::binary);
    if (!input.is_open())
    {
        sqlite3_close(db);
        return false;
    }
    const std::string schema((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    if (schema.empty()
        || !execute_sql(db, "PRAGMA journal_mode=WAL;")
        || !execute_sql(db, "PRAGMA foreign_keys=ON;")
        || !execute_sql(db, "BEGIN IMMEDIATE;")
        || !execute_sql(db, schema)
        || !execute_sql(db, "COMMIT;"))
    {
        execute_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return false;
    }

    const bool valid = validate_existing_schema(db);
    sqlite3_close(db);
    return valid;
}

} // namespace fcpp
