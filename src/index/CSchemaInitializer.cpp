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
#include <system_error>
#include <utility>
#include <vector>

#include <sqlite3.h>

#ifndef FCPP_AGENT_SCHEMA_FILE
#define FCPP_AGENT_SCHEMA_FILE "schema/current.sql"
#endif

namespace fcpp
{

namespace
{

using SExpectedTable = std::pair<const char*, std::vector<const char*>>;

const std::array<SExpectedTable, 13> k_expected_tables = {{
    {"registry_project", {"project_id", "canonical_path", "db_path", "last_opened_at", "created_at"}},
    {"project", {"id", "canonical_path", "name", "build_dir", "last_indexed_at", "index_generation"}},
    {"index_generation", {"generation", "state", "started_at", "finished_at", "file_count", "symbol_count", "error"}},
    {"file", {"id", "path", "generation", "mtime", "size", "hash"}},
    {"symbol", {"id", "generation", "usr", "kind", "name", "qualified_name", "file_id", "parent_id", "parent_usr", "is_definition", "is_declaration", "signature", "access", "flags"}},
    {"symbol_location", {"id", "symbol_id", "file_id", "line", "column", "end_line", "end_column"}},
    {"compile_command", {"id", "file_id", "directory", "arguments", "language"}},
    {"include", {"id", "generation", "from_file_id", "to_path", "resolved_file_id", "is_angle"}},
    {"inheritance", {"id", "generation", "derived_id", "base_id", "base_qname", "access", "is_virtual"}},
    {"override", {"id", "generation", "method_id", "base_method_id"}},
    {"reference", {"id", "generation", "symbol_id", "file_id", "line", "column", "kind"}},
    {"call", {"id", "generation", "caller_id", "callee_id", "callee_qname", "file_id", "line", "column"}},
    {"project_profile", {"id", "original_text", "compact_text"}}
}};

bool validate_existing_schema(sqlite3* db)
{
    for (const SExpectedTable& expected : k_expected_tables)
    {
        const std::string sql = std::string("PRAGMA table_info('") + expected.first + "');";
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
        {
            return false;
        }

        std::vector<std::string> actual_columns;
        int result = SQLITE_OK;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW)
        {
            const unsigned char* name = sqlite3_column_text(statement, 1);
            if (name != nullptr)
            {
                actual_columns.emplace_back(reinterpret_cast<const char*>(name));
            }
        }
        const bool query_ok = result == SQLITE_DONE;
        sqlite3_finalize(statement);
        if (!query_ok || actual_columns.size() != expected.second.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < expected.second.size(); ++i)
        {
            if (actual_columns[i] != expected.second[i])
            {
                return false;
            }
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
