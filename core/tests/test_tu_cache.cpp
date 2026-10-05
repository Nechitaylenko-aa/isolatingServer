#include <catch2/catch_test_macros.hpp>

#include "tu_cache.h"
#include "queries.h"
#include "clang_index.h"
#include <clang-c/Index.h>

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

// USR декларации do_work в api.h (строка 2: "int do_work(int value);").
//
// Намеренно НЕ через queryLocateSymbol/ensure_project_indexed: это тестовый
// bootstrap, который должен добыть usr, не вызывая побочного эффекта
// "весь проект проиндексирован" — иначе тесты вида "первый вызов сканирует,
// второй берёт из кэша" ломаются, потому что к моменту их собственного
// первого вызова кэш уже тёплый от этого самого вызова. Прямой точечный
// парс api.h в обход кэша — ровно то разделение, которого тесты и ждут.
std::string find_do_work_usr(const STestProject& proj)
{
    auto unit = ParsedUnit::parse(proj.header(), {"-std=c++17", "-x", "c++"});
    if (!unit) return "";
    CXFile f = clang_getFile(unit->tu(), proj.header().c_str());
    CXSourceLocation loc = clang_getLocation(unit->tu(), f, 2, 5);
    CXCursor cursor = clang_getCursor(unit->tu(), loc);
    if (clang_Cursor_isNull(cursor) || clang_isInvalid(clang_getCursorKind(cursor))) return "";
    return cursorUSR(cursor);
}

// USR символа в конкретной точке файла — для проверок графа вызовов.
std::string find_usr_at(const std::string& file, unsigned line, unsigned col)
{
    auto unit = ParsedUnit::parse(file, {"-std=c++17", "-x", "c++"});
    if (!unit) return "";
    CXFile f = clang_getFile(unit->tu(), file.c_str());
    CXSourceLocation loc = clang_getLocation(unit->tu(), f, line, col);
    CXCursor cursor = clang_getCursor(unit->tu(), loc);
    if (clang_Cursor_isNull(cursor) || clang_isInvalid(clang_getCursorKind(cursor))) return "";
    return cursorUSR(cursor);
}

} // namespace

TEST_CASE("queryTypeUsage/Complexity/DuplicateFind on the fixture", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    // TYPE_USAGE: do_work используется в alpha.cpp и beta.cpp (>=2 мест)
    json tu = queryTypeUsage(usr, proj.compile_commands());
    REQUIRE(tu.value("ok", false));
    CHECK(tu["locations"].size() >= 2);

    // COMPLEXITY: у alpha.cpp есть метод alpha_sum
    json cx = queryComplexityHotspot(proj.compile_commands(), proj.alpha(), "");
    REQUIRE(cx.value("ok", false));
    REQUIRE(!cx["hotspots"].empty());
    CHECK(cx["hotspots"][0].contains("body_lines"));
    CHECK(cx["hotspots"][0].contains("max_nesting"));

    // DUPLICATE_FIND: в пределах одного файла дублей нет -> групп 0 (не падаем)
    json dup = queryDuplicateFind(proj.compile_commands(), proj.alpha(), "");
    REQUIRE(dup.value("ok", false));
    CHECK(dup["groups"].is_array());
}

TEST_CASE("queryTodoScan/queryDocCheck find markers and undoc headers", "[cache]")
{
    STestProject proj;
    proj.write("sample_quality.cpp",
               "#pragma once\n"                     // без doc-комментария
               "int f() {\n"
               "    // TODO: сделать нормально\n"
               "    int x = 1; // FIXME later\n"
               "    return x;\n"
               "}\n");

    json todo = queryTodoScan(proj.compile_commands(), proj.path("sample_quality.cpp"), "");
    REQUIRE(todo.value("ok", false));
    CHECK(todo["markers"].size() == 2); // TODO + FIXME в одной строке -> один маркер? (разные строки)
    bool sawTodo = false, sawFixme = false;
    for (auto& m : todo["markers"]) {
        if (m.value("marker", "") == "TODO") sawTodo = true;
        if (m.value("marker", "") == "FIXME") sawFixme = true;
    }
    CHECK(sawTodo);
    CHECK(sawFixme);

    json doc = queryDocCheck(proj.compile_commands(), proj.path("sample_quality.cpp"), "");
    REQUIRE(doc.value("ok", false));
    CHECK(doc["checked"].size() == 1);
    CHECK(doc["undocumented"].size() == 1); // заголовочного doc нет
}

// DEPENDENCY_MAP: кто подключает api.h (alpha.cpp), и что подключает alpha.cpp.
TEST_CASE("CTuCache reports file dependents and dependencies", "[cache]")
{
    STestProject proj;
    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), "c:@F@do_work#I#").filesScanned == 2);

    json deps = json::parse(cache.get_file_deps_json(proj.header()));
    REQUIRE(deps.value("ok", false));
    // alpha.cpp включает api.h -> он dependent
    bool alphaDep = false;
    for (auto& d : deps["dependents"]) if (d.get<std::string>().find("alpha.cpp") != std::string::npos) alphaDep = true;
    CHECK(alphaDep);
    // beta.cpp api.h не включает -> не dependent
    bool betaDep = false;
    for (auto& d : deps["dependents"]) if (d.get<std::string>().find("beta.cpp") != std::string::npos) betaDep = true;
    CHECK(!betaDep);

    // alpha.cpp включает api.h -> среди его dependencies
    json a = json::parse(cache.get_file_deps_json(proj.alpha()));
    REQUIRE(a.value("ok", false));
    bool hasHeader = false;
    for (auto& d : a["dependencies"]) if (d.get<std::string>().find("api.h") != std::string::npos) hasHeader = true;
    CHECK(hasHeader);
}

TEST_CASE("CTuCache find_enclosing exposes method extent", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);

    // alpha_sum определён с телом (строки 2..5 в alpha.cpp).
    json m = json::parse(cache.find_enclosing_json(proj.alpha(), 3, 5));
    REQUIRE(m.value("ok", false));
    REQUIRE(!m["enclosing_method"].is_null());
    CHECK(m["enclosing_method"].contains("start_line"));
    CHECK(m["enclosing_method"].contains("end_line"));
    CHECK(m["enclosing_method"]["start_line"].get<int>() <= 3);
    CHECK(m["enclosing_method"]["end_line"].get<int>() >= 3);
}

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

// Баг миграции: PK decl = (usr,file,start_line), поэтому сдвиг строк не
// затирал старую строку (INSERT OR REPLACE добавлял новую), и locate по
// старым координатам продолжал возвращать метод. Правка выше символа обязана
// убирать призрак.
TEST_CASE("CTuCache drops stale decl rows after a line shift", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);

    // до сдвига do_work объявлен в api.h на строке 2
    json before = json::parse(cache.find_enclosing_json(proj.header(), 2, 5));
    REQUIRE(before.value("ok", false));
    REQUIRE(!before["enclosing_method"].is_null());
    CHECK(before["enclosing_method"].value("usr", "") == usr);

    // сдвигаем объявление на строку вниз: api.h -> //pad, #pragma once, do_work
    proj.write("api.h", "// pad\n#pragma once\nint do_work(int value);\n");
    cache.get_refs_for_usr(proj.compile_commands(), usr); // alpha.cpp пересобран

    // старая координата больше НЕ метод (призрак убран)
    json stale = json::parse(cache.find_enclosing_json(proj.header(), 2, 5));
    bool staleIsMethod = !stale["enclosing_method"].is_null() &&
                         stale["enclosing_method"].value("usr", "") == usr;
    CHECK(!staleIsMethod);

    // новая координата — метод
    json fresh = json::parse(cache.find_enclosing_json(proj.header(), 3, 5));
    REQUIRE(!fresh["enclosing_method"].is_null());
    CHECK(fresh["enclosing_method"].value("usr", "") == usr);
}

// Граф вызовов: alpha_sum и beta_sum вызывают do_work -> входящие рёбра есть;
// у alpha_sum исходящее ребро на do_work.
TEST_CASE("CTuCache records and returns call edges", "[cache]")
{
    STestProject proj;
    const std::string doWork = find_do_work_usr(proj);
    const std::string alphaSum = find_usr_at(proj.alpha(), 2, 5); // "int alpha_sum(int a) {"
    REQUIRE(!doWork.empty());
    REQUIRE(!alphaSum.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), doWork).filesScanned == 2);

    // incoming: кто вызывает do_work -> alpha_sum и beta_sum (из разных TU)
    json in = json::parse(cache.get_calls_json(doWork, true));
    REQUIRE(in.value("ok", false));
    CHECK(in.value("direction", "") == "incoming");
    CHECK(in["calls"].size() >= 2);

    // outgoing: кого вызывает alpha_sum -> do_work
    json out = json::parse(cache.get_calls_json(alphaSum, false));
    REQUIRE(out.value("ok", false));
    CHECK(out.value("direction", "") == "outgoing");
    bool foundDoWork = false;
    for (auto& c : out["calls"]) if (c.value("usr", "") == doWork) foundDoWork = true;
    CHECK(foundDoWork);
}

// Fast-path по mtime: перезапись тем же содержимым меняет mtime, но не хэш —
// TU должен остаться валидным (переиндексации нет), т.е. лишнего чтения нет.
TEST_CASE("CTuCache: identical rewrite stays valid via mtime fast-path", "[cache]")
{
    STestProject proj;
    const std::string usr = find_do_work_usr(proj);
    REQUIRE(!usr.empty());

    CTuCache cache(proj.cache_path());
    REQUIRE(cache.opened());
    REQUIRE(cache.get_refs_for_usr(proj.compile_commands(), usr).filesScanned == 2);

    proj.write("alpha.cpp", proj.read("alpha.cpp")); // тот же байт-в-байт контент

    auto after = cache.get_refs_for_usr(proj.compile_commands(), usr);
    REQUIRE(after.ok);
    CHECK(after.filesScanned == 0);
    CHECK(after.fromCache == 2);
}

// Общий на процесс экземпляр: один и тот же путь -> тот же объект.
TEST_CASE("CTuCache::shared returns the same instance per path", "[cache]")
{
    STestProject proj;
    auto a = CTuCache::shared(proj.cache_path());
    auto b = CTuCache::shared(proj.cache_path());
    REQUIRE(a->opened());
    REQUIRE(b->opened());
    CHECK(a.get() == b.get());
}
