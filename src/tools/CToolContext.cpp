/**
 * @file CToolContext.cpp
 * @brief Реализация контекста инструментов.
 */

#include "tools/CToolContext.h"

#include <filesystem>
#include <memory>
#include <utility>

#include <sqlite3.h>

#include "index/CSQLiteIndex.h"
#include "index/CSqlCompilerIndex.h"

namespace fcpp
{

namespace
{

/** @brief Таймаут ожидания снятия блокировки SQLite (мс). */
constexpr int k_busy_timeout_ms = 10000;

} // namespace

CToolContext::CToolContext(std::filesystem::path db_path,
                           std::filesystem::path project_root,
                           IClang* clang)
    : m_db_path(std::move(db_path))
    , m_project_root(std::move(project_root))
    , m_clang(clang)
    , m_db(nullptr)
    , m_graph(nullptr)
    , m_index(nullptr)
{
}

CToolContext::~CToolContext()
{
    // Соединение принадлежит CSQLiteIndex; закрывается через его деструктор.
    m_graph.reset();
    m_index.reset();
    m_db = nullptr;
}

bool CToolContext::open()
{
    if (m_index != nullptr)
    {
        return true;
    }

    // Одно соединение на проект: индекс открывает его, граф использует тот же дескриптор.
    m_index = std::make_unique<CSQLiteIndex>(m_db_path);
    if (!m_index->open_db())
    {
        m_index.reset();
        return false;
    }

    sqlite3* db = m_index->handle();
    sqlite3_busy_timeout(db, k_busy_timeout_ms);
    m_graph = std::make_unique<CSqlCompilerIndex>(db);
    m_db = db;

    return true;
}

CSqlCompilerIndex& CToolContext::graph()
{
    return *m_graph;
}

CSQLiteIndex& CToolContext::index()
{
    return *m_index;
}

const std::filesystem::path& CToolContext::project_root() const
{
    return m_project_root;
}

IClang* CToolContext::clang()
{
    return m_clang;
}

} // namespace fcpp
