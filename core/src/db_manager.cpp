#include "db_manager.h"

#include <filesystem>
#include <utility>

namespace cpptool
{

// ---------------------------------------------------------------- CDbManager

CDbManager::~CDbManager()
{
    close();
}

CDbManager::CDbManager(CDbManager&& other) noexcept
    : db_(other.db_)
    , lastError_(std::move(other.lastError_))
{
    other.db_ = nullptr;
}

CDbManager& CDbManager::operator=(CDbManager&& other) noexcept
{
    if (this != &other)
    {
        close();
        db_ = other.db_;
        lastError_ = std::move(other.lastError_);
        other.db_ = nullptr;
    }
    return *this;
}

bool CDbManager::set_error_from_db(const std::string& context)
{
    lastError_ = context;
    if (db_ != nullptr)
    {
        lastError_ += ": ";
        lastError_ += sqlite3_errmsg(db_);
    }
    return false;
}

bool CDbManager::open(const std::string& path)
{
    close();

    std::error_code ec;
    std::filesystem::path fsPath(path);
    if (fsPath.has_parent_path())
        std::filesystem::create_directories(fsPath.parent_path(), ec);

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK)
    {
        // sqlite3_open отдаёт handle даже при ошибке — его нужно освободить,
        // иначе утечка.
        set_error_from_db("не удалось открыть БД " + path);
        if (db_ != nullptr)
        {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    apply_open_pragmas();
    return true;
}

void CDbManager::apply_open_pragmas()
{
    // WAL: читатель не блокируется писателем — если в БД одновременно смотрит
    // второй процесс cpp-tool или sqlite3 из консоли, это не мешает записи.
    execute("PRAGMA journal_mode=WAL");
    // Ждать освобождения блокировки до 5 секунд вместо мгновенной ошибки.
    execute("PRAGMA busy_timeout=5000");
    // Кэш — не источник истины: он пересчитывается, поэтому полный fsync
    // не нужен, а стоит он на каждый коммит пачки TU.
    execute("PRAGMA synchronous=NORMAL");
    execute("PRAGMA foreign_keys=ON");
}

void CDbManager::close()
{
    if (db_ != nullptr)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool CDbManager::execute(const std::string& sql)
{
    if (db_ == nullptr)
    {
        lastError_ = "БД не открыта";
        return false;
    }
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        lastError_ = "ошибка выполнения SQL: ";
        lastError_ += (errMsg != nullptr) ? errMsg : sqlite3_errmsg(db_);
        if (errMsg != nullptr)
            sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool CDbManager::query_int(const std::string& sql, int64_t& out)
{
    CDbStatement stmt;
    if (!stmt.prepare(db_, sql))
    {
        lastError_ = stmt.last_error();
        return false;
    }
    if (!stmt.step())
        return false;
    out = stmt.column_int(0);
    return true;
}

bool CDbManager::query_string(const std::string& sql, std::string& out)
{
    CDbStatement stmt;
    if (!stmt.prepare(db_, sql))
    {
        lastError_ = stmt.last_error();
        return false;
    }
    if (!stmt.step())
        return false;
    out = stmt.column_string(0);
    return true;
}

bool CDbManager::begin_transaction()
{
    return execute("BEGIN IMMEDIATE");
}

bool CDbManager::commit()
{
    return execute("COMMIT");
}

bool CDbManager::rollback()
{
    return execute("ROLLBACK");
}

// -------------------------------------------------------------- CDbStatement

CDbStatement::~CDbStatement()
{
    if (stmt_ != nullptr)
        sqlite3_finalize(stmt_);
}

CDbStatement::CDbStatement(CDbStatement&& other) noexcept
    : stmt_(other.stmt_)
    , hasRow_(other.hasRow_)
    , lastError_(std::move(other.lastError_))
{
    other.stmt_ = nullptr;
    other.hasRow_ = false;
}

CDbStatement& CDbStatement::operator=(CDbStatement&& other) noexcept
{
    if (this != &other)
    {
        if (stmt_ != nullptr)
            sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        hasRow_ = other.hasRow_;
        lastError_ = std::move(other.lastError_);
        other.stmt_ = nullptr;
        other.hasRow_ = false;
    }
    return *this;
}

bool CDbStatement::prepare(sqlite3* db, const std::string& sql)
{
    if (db == nullptr)
    {
        lastError_ = "БД не открыта";
        return false;
    }
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK)
    {
        lastError_ = std::string("не удалось подготовить выражение: ") + sqlite3_errstr(rc);
        stmt_ = nullptr;
        return false;
    }
    return true;
}

bool CDbStatement::bind_int(int index, int64_t value)
{
    if (stmt_ == nullptr)
    {
        lastError_ = "выражение не подготовлено";
        return false;
    }
    int rc = sqlite3_bind_int64(stmt_, index, value);
    if (rc != SQLITE_OK)
    {
        lastError_ = "не удалось привязать целое";
        return false;
    }
    return true;
}

bool CDbStatement::bind_string(int index, const std::string& value)
{
    if (stmt_ == nullptr)
    {
        lastError_ = "выражение не подготовлено";
        return false;
    }
    // SQLITE_TRANSIENT: SQLite копирует строку сам, поэтому вызывающий не
    // обязан держать её живой до step().
    int rc = sqlite3_bind_text(stmt_, index, value.c_str(),
                               static_cast<int>(value.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK)
    {
        lastError_ = "не удалось привязать строку";
        return false;
    }
    return true;
}

bool CDbStatement::step()
{
    if (stmt_ == nullptr)
    {
        lastError_ = "выражение не подготовлено";
        hasRow_ = false;
        return false;
    }
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)
    {
        hasRow_ = true;
        return true;
    }
    hasRow_ = false;
    if (rc != SQLITE_DONE)
    {
        lastError_ = std::string("ошибка выполнения шага: ") + sqlite3_errstr(rc);
        return false;
    }
    return false;
}

int64_t CDbStatement::column_int(int index) const
{
    if (stmt_ == nullptr)
        return 0;
    return sqlite3_column_int64(stmt_, index);
}

std::string CDbStatement::column_string(int index) const
{
    if (stmt_ == nullptr)
        return "";
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    if (text == nullptr)
        return "";
    return reinterpret_cast<const char*>(text);
}

bool CDbStatement::reset()
{
    if (stmt_ == nullptr)
    {
        lastError_ = "выражение не подготовлено";
        return false;
    }
    int rc = sqlite3_reset(stmt_);
    if (rc != SQLITE_OK)
    {
        lastError_ = std::string("не удалось сбросить выражение: ") + sqlite3_errstr(rc);
        return false;
    }
    hasRow_ = false;
    return true;
}

} // namespace cpptool
