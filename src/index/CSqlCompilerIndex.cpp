/**
 * @file CSqlCompilerIndex.cpp
 * @brief Реализация запросного слоя графа отношений (ADR-11).
 */

#include "index/CSqlCompilerIndex.h"

#include <deque>
#include <set>
#include <string>
#include <vector>

#include <sqlite3.h>

namespace fcpp
{

namespace
{

/** @brief SQL-подзапрос активной generation. */
constexpr const char* k_active_generation =
    "(SELECT generation FROM index_generation WHERE state = 'ACTIVE' "
    "ORDER BY generation DESC LIMIT 1)";

/**
 * @brief Прочитать текстовую колонку.
 * @param statement Подготовленное выражение.
 * @param index Индекс колонки.
 * @return Строка (пустая для NULL).
 */
std::string column_text(sqlite3_stmt* statement, int index)
{
    const unsigned char* text = sqlite3_column_text(statement, index);
    return (text != nullptr) ? reinterpret_cast<const char*>(text) : std::string();
}

} // namespace

CSqlCompilerIndex::CSqlCompilerIndex(sqlite3* db)
    : m_db(db)
{
}

long long CSqlCompilerIndex::symbol_id_by_usr(const std::string& usr) const
{
    if (m_db == nullptr || usr.empty())
    {
        return 0;
    }

    const std::string sql = "SELECT id FROM symbol WHERE usr = ?1 AND generation = "
                            + std::string(k_active_generation) + " ORDER BY id LIMIT 1;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(statement, 1, usr.c_str(), -1, SQLITE_TRANSIENT);

    long long id = 0;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        id = sqlite3_column_int64(statement, 0);
    }
    sqlite3_finalize(statement);
    return id;
}

std::string CSqlCompilerIndex::symbol_usr_by_id(long long id) const
{
    if (m_db == nullptr || id <= 0)
    {
        return {};
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT usr FROM symbol WHERE id = ?1;", -1, &statement, nullptr) != SQLITE_OK)
    {
        return {};
    }
    sqlite3_bind_int64(statement, 1, id);

    std::string usr;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        usr = column_text(statement, 0);
    }
    sqlite3_finalize(statement);
    return usr;
}

std::vector<SBaseType> CSqlCompilerIndex::base_types(const std::string& class_usr) const
{
    std::vector<SBaseType> bases;
    if (m_db == nullptr)
    {
        return bases;
    }

    const std::string sql =
        "SELECT base_id, base_qname, access, is_virtual FROM inheritance "
        "WHERE derived_id = (SELECT id FROM symbol WHERE usr = ?1 AND generation = "
        + std::string(k_active_generation) + " ORDER BY id LIMIT 1) AND generation = "
        + std::string(k_active_generation) + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return bases;
    }
    sqlite3_bind_text(statement, 1, class_usr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SBaseType base;
        if (sqlite3_column_type(statement, 0) != SQLITE_NULL)
        {
            base.m_usr = symbol_usr_by_id(sqlite3_column_int64(statement, 0));
        }
        base.m_qname = column_text(statement, 1);
        base.m_access = column_text(statement, 2);
        base.m_is_virtual = (sqlite3_column_int(statement, 3) != 0);
        bases.push_back(std::move(base));
    }
    sqlite3_finalize(statement);
    return bases;
}

std::vector<SBaseType> CSqlCompilerIndex::ancestors(const std::string& class_usr) const
{
    std::vector<SBaseType> chain;
    std::set<std::string> visited;
    std::string current = class_usr;

    while (!current.empty() && visited.find(current) == visited.end())
    {
        visited.insert(current);
        const std::vector<SBaseType> bases = base_types(current);
        std::string next;
        for (const SBaseType& base : bases)
        {
            chain.push_back(base);
            if (next.empty() && !base.m_usr.empty())
            {
                next = base.m_usr;
            }
        }
        current = next;
    }

    return chain;
}

std::vector<SBaseType> CSqlCompilerIndex::derived_types(const std::string& class_usr) const
{
    std::vector<SBaseType> derived;
    if (m_db == nullptr)
    {
        return derived;
    }

    const std::string sql =
        "SELECT derived_id FROM inheritance "
        "WHERE base_id = (SELECT id FROM symbol WHERE usr = ?1 AND generation = "
        + std::string(k_active_generation) + " ORDER BY id LIMIT 1) AND generation = "
        + std::string(k_active_generation) + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return derived;
    }
    sqlite3_bind_text(statement, 1, class_usr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        SBaseType entry;
        entry.m_usr = symbol_usr_by_id(sqlite3_column_int64(statement, 0));
        derived.push_back(std::move(entry));
    }
    sqlite3_finalize(statement);
    return derived;
}

std::vector<std::string> CSqlCompilerIndex::overrides(const std::string& method_usr) const
{
    std::vector<std::string> result;
    if (m_db == nullptr)
    {
        return result;
    }

    const std::string sql =
        "SELECT method_id FROM override WHERE base_method_id = "
        "(SELECT id FROM symbol WHERE usr = ?1 AND generation = " + std::string(k_active_generation)
        + " ORDER BY id LIMIT 1) AND generation = " + std::string(k_active_generation) + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_text(statement, 1, method_usr.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        result.push_back(symbol_usr_by_id(sqlite3_column_int64(statement, 0)));
    }
    sqlite3_finalize(statement);
    return result;
}

std::string CSqlCompilerIndex::base_method(const std::string& method_usr) const
{
    if (m_db == nullptr)
    {
        return {};
    }

    const std::string sql =
        "SELECT base_method_id FROM override WHERE method_id = "
        "(SELECT id FROM symbol WHERE usr = ?1 AND generation = " + std::string(k_active_generation)
        + " ORDER BY id LIMIT 1) AND generation = " + std::string(k_active_generation) + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return {};
    }
    sqlite3_bind_text(statement, 1, method_usr.c_str(), -1, SQLITE_TRANSIENT);

    std::string base;
    if (sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_type(statement, 0) != SQLITE_NULL)
    {
        base = symbol_usr_by_id(sqlite3_column_int64(statement, 0));
    }
    sqlite3_finalize(statement);
    return base;
}

std::vector<std::string> CSqlCompilerIndex::dispatch_targets(const std::string& method_usr) const
{
    std::vector<std::string> result;
    if (method_usr.empty())
    {
        return result;
    }

    std::string root = method_usr;
    std::set<std::string> guard;
    while (true)
    {
        if (guard.find(root) != guard.end())
        {
            break;
        }
        guard.insert(root);
        const std::string base = base_method(root);
        if (base.empty())
        {
            break;
        }
        root = base;
    }

    std::set<std::string> visited;
    std::deque<std::string> queue;
    queue.push_back(root);
    visited.insert(root);

    while (!queue.empty())
    {
        const std::string current = queue.front();
        queue.pop_front();
        result.push_back(current);

        for (const std::string& overridden : overrides(current))
        {
            if (visited.find(overridden) == visited.end())
            {
                visited.insert(overridden);
                queue.push_back(overridden);
            }
        }
    }

    return result;
}

std::vector<std::string> CSqlCompilerIndex::file_includes(long long file_id) const
{
    std::vector<std::string> result;
    if (m_db == nullptr)
    {
        return result;
    }

    const std::string sql =
        "SELECT to_path FROM include WHERE from_file_id = ?1 AND generation = "
        + std::string(k_active_generation) + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_int64(statement, 1, file_id);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        result.push_back(column_text(statement, 0));
    }
    sqlite3_finalize(statement);
    return result;
}

std::vector<std::string> CSqlCompilerIndex::included_by(long long file_id) const
{
    std::vector<std::string> result;
    if (m_db == nullptr)
    {
        return result;
    }

    const std::string sql =
        "SELECT DISTINCT f.path FROM include i JOIN file f ON f.id = i.from_file_id "
        "WHERE i.resolved_file_id = ?1 AND i.generation = " + std::string(k_active_generation) + ";";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        return result;
    }
    sqlite3_bind_int64(statement, 1, file_id);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        result.push_back(column_text(statement, 0));
    }
    sqlite3_finalize(statement);
    return result;
}

} // namespace fcpp
