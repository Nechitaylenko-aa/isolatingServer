#include <catch2/catch_test_macros.hpp>

#include "tu_cache.h"
#include "queries.h"

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace cpptool;
namespace fs = std::filesystem;

namespace
{

// Временный C++ проект с compile_commands.json, написанным руками — cmake не
// нужен, важны только флаги, которые получит libclang.
//   api.h     — заголовок внутри project root, объявляет do_work
//   alpha.cpp — подключает api.h, вызывает do_work дважды
//   beta.cpp  — независимый TU, тоже вызывает do_work, api.h не подключает
class STestProject
{
public:
    STestProject()
    {
        root_ = fs::temp_directory_path() / ("cpp_tool_tu_" + std::to_string(::rand()));
        fs::create_directories(root_);

        write("api.h",
              "#pragma once\n"
              "int do_work(int value);\n");
        write("alpha.cpp",
              "#include \"api.h\"\n"
              "int alpha_sum(int a) {\n"
              "    int r = do_work(a);\n"
              "    return r + do_work(a);\n"
              "}\n");
        write("beta.cpp",
              "int do_work(int value);\n"
              "int beta_sum(int b) {\n"
              "    return do_work(b);\n"
              "}\n");

        write_compile_commands("-std=c++17");
    }

    ~STestProject()
    {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    const fs::path& root() const { return root_; }
    std::string path(const std::string& name) const { return (root_ / name).string(); }
    std::string cache_path() const { return (root_ / ".cpp-tool-cache" / "index.db").string(); }
    std::string compile_commands() const { return path("compile_commands.json"); }
    std::string alpha() const { return path("alpha.cpp"); }
    std::string beta() const { return path("beta.cpp"); }
    std::string header() const { return path("api.h"); }

    void write(const std::string& name, const std::string& content) const
    {
        std::ofstream out(root_ / name, std::ios::trunc);
        out << content;
    }

    std::string read(const std::string& name) const
    {
        std::ifstream in(root_ / name);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void append(const std::string& name, const std::string& extra) const
    {
        write(name, read(name) + extra);
    }

    void remove(const std::string& name) const
    {
        std::error_code ec;
        fs::remove(root_ / name, ec);
    }

    // Переписывает compile_commands.json с другими флагами компиляции —
    // нужен тесту на инвалидацию по flags_hash без изменения содержимого файлов.
    void write_compile_commands(const std::string& extraFlags) const
    {
        json entries = json::array();
        for (const char* src : {"alpha.cpp", "beta.cpp"})
        {
            json entry;
            entry["directory"] = root_.string();
            entry["file"] = (root_ / src).string();
            entry["command"] = "c++ " + extraFlags + " -c " + (root_ / src).string();
            entries.push_back(entry);
        }
        std::ofstream out(root_ / "compile_commands.json", std::ios::trunc);
        out << entries.dump(2);
    }

private:
    fs::path root_;
};

int64_t count_rows(const std::string& dbPath, const std::string& table)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        return -1;
    }
    const std::string sql = "SELECT count(*) FROM " + table;
    sqlite3_stmt* stmt = nullptr;
    int64_t count = -1;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

// Порча метаданных извне процесса — имитирует апгрейд libclang между запусками.
void poke_meta_value(const std::string& dbPath, const std::string& key, const std::string& value)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        return;
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE meta SET value = ? WHERE key = ?", -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void set_user_version(const std::string& dbPath, int version)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        return;
    }
    const std::string sql = "PRAGMA user_version=" + std::to_string(version);
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    sqlite3_close(db);
}

// USR do_work берём из разбора api.h в режиме C++ (-x c++: без него clang
// разбирает .h как C-заголовок и разбор падает). Именно декларация, а не
// вызов: у DeclRefExpr в месте вызова cursorUSR пустой, а USR декларации
// name-based и совпадает с тем, что ссылается на символ из других TU.
std::string find_do_work_usr(const STestProject& proj)
{
    json locate = queryLocateSymbol(proj.header(), 2, 5, {"-std=c++17", "-x", "c++"});
    if (!locate.value("ok", false))
        return "";
    return locate.value("cursor", json::object()).value("usr", "");
}

} // namespace

TEST_CASE("CTuCache opens and creates schema", "[cache]")
{
    STestProject proj;
    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    CHECK(fs::exists(proj.cache_path()));
    CHECK(count_rows(proj.cache_path(), "tu") == 0);
}

TEST_CASE("CTuCache scans project once, then serves from cache", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());

    auto first = cache.get_refs_for_usr(proj.compile_commands(), usr);
    REQUIRE(first.ok);
    CHECK(first.filesTotal == 2);
    CHECK(first.filesScanned == 2);
    CHECK(first.fromCache == 0);
    CHECK(first.distinctFilesWithRefs == 2); // do_work встречается и в alpha, и в beta
    CHECK(first.totalRefs > 0);

    auto second = cache.get_refs_for_usr(proj.compile_commands(), usr);
    REQUIRE(second.ok);
    CHECK(second.filesScanned == 0);
    CHECK(second.fromCache == 2);
    CHECK(second.totalRefs == first.totalRefs); // ничего не менялось — результат стабилен
}

TEST_CASE("CTuCache survives reopen of the database", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    int first_total = 0;
    {
        CTuCache cache(proj.cache_path());
        REQUIRE(cache.opened());
        auto res = cache.get_refs_for_usr(proj.compile_commands(), usr);
        REQUIRE(res.filesScanned == 2);
        first_total = res.totalRefs;
    }
    {
        CTuCache cache(proj.cache_path());
        REQUIRE(cache.opened());
        auto res = cache.get_refs_for_usr(proj.compile_commands(), usr);
        REQUIRE(res.ok);
        CHECK(res.filesScanned == 0);
        CHECK(res.fromCache == 2);
        CHECK(res.totalRefs == first_total);
    }
}

// Баг №1 из диагностики: правка заголовка обязана инвалидировать TU, который
// его подключает. Раньше изменение api.h не отслеживалось вообще.
TEST_CASE("CTuCache invalidates the TU that includes the edited header", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);

    proj.append("api.h", "// touched, no semantic change\n");

    auto after = cache.get_refs_for_usr(proj.compile_commands(), usr);
    REQUIRE(after.ok);
    CHECK(after.filesScanned == 1); // alpha.cpp подключает api.h — пересобран
    CHECK(after.fromCache == 1);    // beta.cpp не подключает — остался из кэша
}

TEST_CASE("CTuCache sees a new reference added via the header-including TU", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    const int baseline = cache.get_refs_for_usr(proj.compile_commands(), usr).totalRefs;

    proj.append("alpha.cpp",
        "int alpha_more(int a) {\n"
        "    return do_work(a);\n"
        "}\n");

    auto after = cache.get_refs_for_usr(proj.compile_commands(), usr);
    REQUIRE(after.ok);
    CHECK(after.filesScanned == 1);
    CHECK(after.totalRefs > baseline); // новый вызов добавил вхождения
}

// Осознанно принятое ограничение: заголовки ВНЕ project root не отслеживаются.
TEST_CASE("CTuCache does not invalidate an unrelated TU", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);

    // beta.cpp не подключает api.h — правка заголовка его не касается.
    proj.append("api.h", "// unrelated edit\n");

    auto after = cache.get_refs_for_usr(proj.compile_commands(), usr);
    REQUIRE(after.ok);
    CHECK(after.fromCache == 1);    // beta.cpp — валиден
    CHECK(after.filesScanned == 1); // alpha.cpp — пересобран (зависит от api.h)
}

TEST_CASE("CTuCache invalidates on compiler flags change", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).fromCache == 2);

    // Содержимое файлов не менялось — поменялись только флаги в compile_commands.json.
    proj.write_compile_commands("-std=c++17 -DUNUSED_MACRO=1");

    auto after = cache.get_refs_for_usr(proj.compile_commands(), usr);
    REQUIRE(after.ok);
    CHECK(after.filesScanned == 2); // оба TU грязные из-за flags_hash
}

TEST_CASE("CTuCache treats a deleted tracked header as a dirty TU, not a crash", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);

    proj.remove("api.h"); // alpha.cpp теперь не соберётся — tu_dep на него ссылается

    auto after = cache.get_refs_for_usr(proj.compile_commands(), usr);
    CHECK(after.ok); // не падаем; alpha.cpp просто не парсится и пропускается
}

TEST_CASE("CTuCache returns zero refs for unknown USR without error", "[cache]")
{
    STestProject proj;
    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());

    auto res = cache.get_refs_for_usr(proj.compile_commands(), "c:@F@no_such_symbol#I#");
    REQUIRE(res.ok);
    CHECK(res.totalRefs == 0);
    CHECK(res.distinctFilesWithRefs == 0);
    // Проект всё равно весь просканирован — отсутствие совпадений не ошибка.
    CHECK(res.filesScanned == 2);
}

TEST_CASE("CTuCache reports a project-level error when compile_commands is missing", "[cache]")
{
    STestProject proj;
    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());

    auto res = cache.get_refs_for_usr(proj.path("no_such_compile_commands.json"), "c:@F@x#");
    CHECK(res.ok == false);
    CHECK(!res.error.empty());
}

// Баг №5 из диагностики: смена версии libclang должна ломать валидность кэша
// целиком, а не оставлять старые записи похожими на валидные.
TEST_CASE("CTuCache wipes everything when libclang version in meta differs", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());
    const std::string dbPath = proj.cache_path();

    {
        CTuCache cache(dbPath);
        REQUIRE(cache.opened());
        REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);
    }

    poke_meta_value(dbPath, "libclang_version", "clang version 1.0.0 (fake, previous toolchain)");

    {
        CTuCache cache(dbPath);
        REQUIRE(cache.opened());
        CHECK(count_rows(dbPath, "tu") == 0); // кэш стёрт при открытии, не по частям

        auto res = cache.get_refs_for_usr(proj.compile_commands(), usr);
        REQUIRE(res.ok);
        CHECK(res.filesScanned == 2); // перепарсено заново, а не тихо "0 ссылок"
    }
}

// Баг №4 из диагностики: смена формата схемы (PRAGMA user_version) должна
// ломать валидность кэша целиком.
TEST_CASE("CTuCache wipes everything when schema version differs", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());
    const std::string dbPath = proj.cache_path();

    {
        CTuCache cache(dbPath);
        REQUIRE(cache.opened());
        REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);
    }

    set_user_version(dbPath, 999); // имитирует несовпадение версии схемы

    {
        CTuCache cache(dbPath);
        REQUIRE(cache.opened());
        CHECK(count_rows(dbPath, "tu") == 0);

        auto res = cache.get_refs_for_usr(proj.compile_commands(), usr);
        REQUIRE(res.ok);
        CHECK(res.filesScanned == 2);
    }
}

TEST_CASE("CTuCache opening the same fresh database twice does not loop or wipe", "[cache]")
{
    // Регрессия на конкретный баг реализации: свежая БД имеет user_version=0,
    // что отличается от текущей версии схемы — без выставления версии ПОСЛЕ
    // DROP, но ДО повторного создания таблиц, это приводило к бесконечной
    // рекурсии в ensure_schema на каждом открытии, в том числе первом.
    STestProject proj;
    CTuCache first(proj.cache_path());
    REQUIRE(first.opened());
    CTuCache second(proj.cache_path());
    REQUIRE(second.opened());
}
