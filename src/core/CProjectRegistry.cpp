/**
 * @file CProjectRegistry.cpp
 * @brief Реализация реестра проектов (ADR-03).
 */

#include "core/CProjectRegistry.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <sqlite3.h>

namespace fcpp
{

namespace
{

/** @brief Таймаут ожидания снятия блокировки SQLite (мс). */
constexpr int k_busy_timeout_ms = 10000;

/**
 * @brief Извлечь текстовую колонку в std::string.
 * @param statement Подготовленное выражение.
 * @param column_index Индекс колонки.
 * @return Значение колонки (пустая строка для NULL).
 */
std::string column_text(sqlite3_stmt* statement, int column_index)
{
    const unsigned char* text = sqlite3_column_text(statement, column_index);
    if (text == nullptr)
    {
        return {};
    }
    return reinterpret_cast<const char*>(text);
}

/**
 * @brief Прочитать строку результата в структуру проекта.
 * @param statement Подготовленное выражение с текущей строкой.
 * @return Структура проекта.
 */
SProject read_project(sqlite3_stmt* statement)
{
    SProject project;
    project.m_project_id = column_text(statement, 0);
    project.m_canonical_path = column_text(statement, 1);
    project.m_db_path = column_text(statement, 2);
    project.m_last_opened_at = sqlite3_column_int64(statement, 3);
    project.m_name = project.m_canonical_path.filename().string();
    return project;
}

} // namespace

CProjectRegistry::CProjectRegistry(std::filesystem::path db_path)
    : m_db(nullptr)
    , m_db_path(std::move(db_path))
{
}

CProjectRegistry::~CProjectRegistry()
{
    close_db();
}

bool CProjectRegistry::exec_sql(const char* sql) const
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

bool CProjectRegistry::open_db()
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

bool CProjectRegistry::close_db()
{
    if (m_db == nullptr)
    {
        return true;
    }

    const int result_code = sqlite3_close(m_db);
    m_db = nullptr;
    return result_code == SQLITE_OK;
}

bool CProjectRegistry::upsert(const SProject& project)
{
    if (m_db == nullptr)
    {
        return false;
    }

    const char* sql =
        "INSERT INTO registry_project (project_id, canonical_path, db_path, last_opened_at, created_at) "
        "VALUES (?1, ?2, ?3, ?4, ?4) "
        "ON CONFLICT(project_id) DO UPDATE SET "
        "canonical_path = excluded.canonical_path, "
        "db_path = excluded.db_path, "
        "last_opened_at = excluded.last_opened_at;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }

    const std::string canonical = project.m_canonical_path.string();
    const std::string db_path = project.m_db_path.string();

    sqlite3_bind_text(statement, 1, project.m_project_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, canonical.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, db_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 4, project.m_last_opened_at);

    const int step_result = sqlite3_step(statement);
    sqlite3_finalize(statement);

    return step_result == SQLITE_DONE;
}

std::optional<SProject> CProjectRegistry::find_by_path(const std::filesystem::path& canonical_path) const
{
    if (m_db == nullptr)
    {
        return std::nullopt;
    }

    const char* sql =
        "SELECT project_id, canonical_path, db_path, last_opened_at "
        "FROM registry_project WHERE canonical_path = ?1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    const std::string canonical = canonical_path.string();
    sqlite3_bind_text(statement, 1, canonical.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<SProject> result;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        result = read_project(statement);
    }

    sqlite3_finalize(statement);
    return result;
}

bool CProjectRegistry::erase(const std::string& project_id)
{
    if (m_db == nullptr)
    {
        return false;
    }

    const char* sql = "DELETE FROM registry_project WHERE project_id = ?1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(statement, 1, project_id.c_str(), -1, SQLITE_TRANSIENT);

    const int step_result = sqlite3_step(statement);
    sqlite3_finalize(statement);

    return step_result == SQLITE_DONE;
}

std::vector<SProject> CProjectRegistry::list() const
{
    std::vector<SProject> projects;
    if (m_db == nullptr)
    {
        return projects;
    }

    const char* sql =
        "SELECT project_id, canonical_path, db_path, last_opened_at "
        "FROM registry_project ORDER BY last_opened_at DESC;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        return projects;
    }

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        projects.push_back(read_project(statement));
    }

    sqlite3_finalize(statement);
    return projects;
}

} // namespace fcpp
