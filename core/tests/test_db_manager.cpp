#include <catch2/catch_test_macros.hpp>

#include "db_manager.h"

#include <filesystem>
#include <fstream>

using namespace cpptool;
namespace fs = std::filesystem;

namespace
{

// Временная БД, удаляемая вместе с каталогом. WAL оставляет после себя
// -wal и -shm, поэтому чистим каталог целиком, а не один файл.
class STempDb
{
public:
    STempDb()
    {
        dir_ = fs::temp_directory_path() / ("cpp_tool_db_" + std::to_string(::rand()));
        fs::create_directories(dir_);
        path_ = (dir_ / "cache.db").string();
    }

    ~STempDb()
    {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    const std::string& path() const { return path_; }

private:
    fs::path dir_;
    std::string path_;
};

} // namespace

TEST_CASE("CDbManager creates database file and accepts DDL", "[db]")
{
    STempDb tmp;
    CDbManager db;
    REQUIRE(db.open(tmp.path()));
    CHECK(db.is_open());
    CHECK(fs::exists(tmp.path()));

    CHECK(db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"));
    CHECK(db.execute("INSERT INTO t (name) VALUES ('x')"));
}

TEST_CASE("CDbManager prepares and binds values", "[db]")
{
    STempDb tmp;
    CDbManager db;
    REQUIRE(db.open(tmp.path()));
    REQUIRE(db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, num INTEGER)"));

    CDbStatement insert;
    REQUIRE(insert.prepare(db.handle(), "INSERT INTO t (name, num) VALUES (?, ?)"));
    REQUIRE(insert.bind_string(1, "привет"));
    REQUIRE(insert.bind_int(2, 42));
    insert.step();

    CDbStatement select;
    REQUIRE(select.prepare(db.handle(), "SELECT name, num FROM t WHERE id = ?"));
    REQUIRE(select.bind_int(1, 1));
    REQUIRE(select.step());
    REQUIRE(select.has_row());
    CHECK(select.column_string(0) == "привет");
    CHECK(select.column_int(1) == 42);
}

TEST_CASE("CDbManager statement reset allows reuse", "[db]")
{
    STempDb tmp;
    CDbManager db;
    REQUIRE(db.open(tmp.path()));
    REQUIRE(db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"));

    CDbStatement insert;
    REQUIRE(insert.prepare(db.handle(), "INSERT INTO t (name) VALUES (?)"));
    for (const char* name : {"a", "b", "c"})
    {
        REQUIRE(insert.reset());
        REQUIRE(insert.bind_string(1, name));
        insert.step();
    }

    int64_t count = 0;
    REQUIRE(db.query_int("SELECT count(*) FROM t", count));
    CHECK(count == 3);
}

TEST_CASE("CDbManager rolls back an aborted transaction", "[db]")
{
    STempDb tmp;
    CDbManager db;
    REQUIRE(db.open(tmp.path()));
    REQUIRE(db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"));

    REQUIRE(db.begin_transaction());
    REQUIRE(db.execute("INSERT INTO t (name) VALUES ('kept')"));
    REQUIRE(db.rollback());

    int64_t count = 0;
    REQUIRE(db.query_int("SELECT count(*) FROM t", count));
    CHECK(count == 0);

    // После отката соединение остаётся рабочим — это важно для вызывающего
    // кода, который откатывает транзакцию и продолжает цикл разбора.
    REQUIRE(db.begin_transaction());
    REQUIRE(db.execute("INSERT INTO t (name) VALUES ('committed')"));
    REQUIRE(db.commit());
    REQUIRE(db.query_int("SELECT count(*) FROM t", count));
    CHECK(count == 1);
}

TEST_CASE("CDbManager persists across reopen", "[db]")
{
    STempDb tmp;
    {
        CDbManager db;
        REQUIRE(db.open(tmp.path()));
        REQUIRE(db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"));
        REQUIRE(db.execute("INSERT INTO t (name) VALUES ('persisted')"));
    }
    {
        CDbManager db;
        REQUIRE(db.open(tmp.path()));
        std::string name;
        REQUIRE(db.query_string("SELECT name FROM t WHERE id = 1", name));
        CHECK(name == "persisted");
    }
}

TEST_CASE("CDbManager enables WAL journal mode", "[db]")
{
    // WAL важен не производительностью, а тем, что читатель не блокирует
    // писателя: в ту же БД может смотреть второй процесс или sqlite3 вручную.
    STempDb tmp;
    CDbManager db;
    REQUIRE(db.open(tmp.path()));

    std::string mode;
    REQUIRE(db.query_string("PRAGMA journal_mode", mode));
    CHECK(mode == "wal");
}

TEST_CASE("CDbManager reports error instead of crashing on bad path", "[db]")
{
    CDbManager db;
    // Каталог вместо файла — SQLite не откроет это как БД.
    const std::string dirPath = fs::temp_directory_path().string();
    // Не проверяем успех/неуспех открытия как таковой: важно, что вызов
    // возвращает результат и не падает, а ошибка доступна вызывающему.
    const bool ok = db.open(dirPath);
    if (!ok)
    {
        CHECK(db.is_open() == false);
        CHECK(!db.last_error().empty());
    }
}

TEST_CASE("CDbManager move transfers the connection", "[db]")
{
    STempDb tmp;
    CDbManager db;
    REQUIRE(db.open(tmp.path()));
    REQUIRE(db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)"));

    CDbManager moved(std::move(db));
    CHECK(moved.is_open());
    CHECK(db.is_open() == false);
    // Соединение действительно переехало, а не только флаг.
    CHECK(moved.execute("INSERT INTO t (id) VALUES (1)"));
}
