#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "execute_service.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace cppagent;
namespace fs = std::filesystem;

static std::string readFile(const std::string& p){
    std::ifstream in(p); std::ostringstream ss; ss<<in.rdbuf(); return ss.str();
}
static void writeFile(const std::string& p, const std::string& c){
    std::ofstream out(p, std::ios::trunc); out<<c;
}

TEST_CASE("ExecuteService apply: replace oldText", "[execute]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "sample.cpp").string();
    writeFile(file, "int x = 1;\nint y = 2;\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "int x = 1;", "int x = 42;"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    REQUIRE(r.appliedFiles.size()==1);
    REQUIRE(!r.checkpointId.empty());
    CHECK(readFile(file)=="int x = 42;\nint y = 2;\n");
    // undo
    std::string err;
    REQUIRE(svc.undo(r.checkpointId, err));
    CHECK(readFile(file)=="int x = 1;\nint y = 2;\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: insert when oldText empty", "[execute]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "a.cpp").string();
    writeFile(file, "line1\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "", "line2\n"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    CHECK(readFile(file)=="line1\nline2\n");
    std::string err; REQUIRE(svc.undo(r.checkpointId, err));
    CHECK(readFile(file)=="line1\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: oldText not found -> fail", "[execute]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "b.cpp").string();
    writeFile(file, "hello\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "not_exist", "x"};
    auto r = svc.apply({e});
    CHECK(!r.ok);
    CHECK(r.error.find("oldText")!=std::string::npos);
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: file not found -> fail", "[execute]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    ExecuteService svc((tmp / "chk").string());
    EditAction e{(tmp / "nope.cpp").string(), "a", "b"};
    auto r = svc.apply({e});
    CHECK(!r.ok);
    CHECK(r.error.find("не найден")!=std::string::npos);
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: empty edits -> ok no checkpoint", "[execute]"){
    ExecuteService svc("/tmp/cpp-tool-checkpoint-test-empty");
    auto r = svc.apply({});
    CHECK(r.ok);
    CHECK(r.checkpointId.empty());
    CHECK(r.appliedFiles.empty());
}

TEST_CASE("ExecuteService undo: unknown id -> fail", "[execute]"){
    ExecuteService svc("/tmp/cpp-tool-checkpoint-test-undo");
    std::string err;
    CHECK(!svc.undo("no-such-id-123", err));
    CHECK(!err.empty());
}

TEST_CASE("ExecuteService apply: fuzzy match on whitespace differences", "[execute][fuzzy]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_ws_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "ws.cpp").string();
    writeFile(file, "int  x   =   1;\nint y = 2;\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "int x = 1;", "int x = 42;"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    REQUIRE(r.matchInfo.size() == 1);
    CHECK(r.matchInfo[0].value("match_type", "") == "normalized");
    CHECK(r.matchInfo[0].value("score", 0.0) == Catch::Approx(0.9));
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: fuzzy prefix match when model collapses newlines", "[execute][fuzzy]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_ws2_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "ws2.cpp").string();
    writeFile(file, "void process(\n    int a,\n    int b\n) {\n    return a + b;\n}\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "void process( int a, int b ) { return a + b; }", "void process(int a, int b) { return a + b + 1; }"};
    auto r = svc.apply({e});
    if (r.ok) {
        REQUIRE(r.matchInfo.size() == 1);
        std::string mt = r.matchInfo[0].value("match_type", "");
        CHECK((mt == "normalized" || mt == "prefix"));
    } else {
        CHECK(r.error.find("oldText не найден") != std::string::npos);
    }
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: exact match reports exact", "[execute][fuzzy]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_ex_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "ex.cpp").string();
    writeFile(file, "int x = 1;\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "int x = 1;", "int x = 2;"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    REQUIRE(r.matchInfo.size() == 1);
    CHECK(r.matchInfo[0].value("match_type", "") == "exact");
    CHECK(r.matchInfo[0].value("score", 0.0) == Catch::Approx(1.0));
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: empty oldText reports insert", "[execute][fuzzy]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_ins_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "ins.cpp").string();
    writeFile(file, "line1\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "", "// appended\n"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    REQUIRE(r.matchInfo.size() == 1);
    CHECK(r.matchInfo[0].value("match_type", "") == "insert");
    CHECK(readFile(file) == "line1\n// appended\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: multi-file", "[execute]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string f1=(tmp/"f1.cpp").string(), f2=(tmp/"f2.cpp").string();
    writeFile(f1, "AAA\n"); writeFile(f2, "BBB\n");
    ExecuteService svc((tmp/"chk").string());
    auto r = svc.apply({{f1,"AAA","X"},{f2,"BBB","Y"}});
    REQUIRE(r.ok);
    CHECK(r.appliedFiles.size()==2);
    CHECK(readFile(f1)=="X\n"); CHECK(readFile(f2)=="Y\n");
    std::string err; REQUIRE(svc.undo(r.checkpointId, err));
    CHECK(readFile(f1)=="AAA\n"); CHECK(readFile(f2)=="BBB\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: failure on second edit must not leave first file changed", "[execute][atomic]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_atomic_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string f1 = (tmp / "p1.cpp").string();
    std::string f2 = (tmp / "p2.cpp").string();
    writeFile(f1, "AAA\n");
    writeFile(f2, "BBB\n");
    ExecuteService svc((tmp / "chk").string());
    // Первая правка применима, вторая — нет: apply обязан отработать атомарно.
    auto r = svc.apply({{f1, "AAA", "X"}, {f2, "NOT_THERE", "Y"}});
    CHECK(!r.ok);
    CHECK(r.error.find("oldText") != std::string::npos);
    // Файл, куда правка успела примениться, должен остаться прежним.
    CHECK(readFile(f1) == "AAA\n");
    CHECK(readFile(f2) == "BBB\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: picks occurrence nearest to hintLine, not first", "[execute][fuzzy][hint]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_hint_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "dup.cpp").string();
    // Одинаковая строка встречается трижды: на 2-й, 5-й и 9-й строках.
    writeFile(file,
        "int a = 0;\n"
        "return done();\n"
        "int b = 1;\n"
        "int c = 2;\n"
        "return done();\n"
        "int d = 3;\n"
        "int e = 4;\n"
        "int f = 5;\n"
        "return done();\n");
    ExecuteService svc((tmp / "chk").string());

    // Курсор на 8-й строке -> должны заменить вхождение на 9-й, а не на 2-й.
    EditAction e{file, "return done();", "return done_replaced();", 8};
    auto r = svc.apply({e});
    REQUIRE(r.ok);

    // Точное сравнение: различает, КАКОЕ вхождение заменено (вхождения 2 и 5 должны остаться).
    CHECK(readFile(file) ==
        "int a = 0;\n"
        "return done();\n"
        "int b = 1;\n"
        "int c = 2;\n"
        "return done();\n"
        "int d = 3;\n"
        "int e = 4;\n"
        "int f = 5;\n"
        "return done_replaced();\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: reindents newText when model drops leading indent", "[execute][indent]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_ind_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "ind.cpp").string();
    // Метод с отступом 4 у тела. oldText — с отступом, newText — без (как отдаёт модель).
    writeFile(file,
        "void f() {\n"
        "    int x = 1;\n"
        "    return;\n"
        "}\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "    int x = 1;\n    return;", "int x = 2;\nreturn;"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    // newText должен получить тот же отступ 4, что был в файле.
    CHECK(readFile(file) ==
        "void f() {\n"
        "    int x = 2;\n"
        "    return;\n"
        "}\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: removes excess indent when oldText over-indented", "[execute][indent]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_ind2_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "ind2.cpp").string();
    // В файле отступ 4, oldText пришёл с 8 — замену надо выровнять на 4, не удваивать.
    writeFile(file,
        "void f() {\n"
        "    int x = 1;\n"
        "}\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "        int x = 1;", "        int x = 2;"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    CHECK(readFile(file) ==
        "void f() {\n"
        "    int x = 2;\n"
        "}\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: preserves relative inner indent in multi-line newText", "[execute][indent]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_ind3_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "ind3.cpp").string();
    writeFile(file,
        "void f() {\n"
        "    if (a) {\n"
        "        b();\n"
        "    }\n"
        "}\n");
    ExecuteService svc((tmp / "chk").string());
    // oldText с отступом 4, newText без внешнего, но с относительным внутренним (+4).
    EditAction e{file,
        "    if (a) {\n        b();\n    }",
        "if (c) {\n    d();\n}"};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    // Внешний отступ 4 добавлен, относительный (4 между if и d()) сохранён.
    CHECK(readFile(file) ==
        "void f() {\n"
        "    if (c) {\n"
        "        d();\n"
        "    }\n"
        "}\n");
    fs::remove_all(tmp);
}

TEST_CASE("ExecuteService apply: hintLine 0 keeps first-occurrence behaviour", "[execute][fuzzy][hint]"){
    fs::path tmp = fs::temp_directory_path() / ("cpp_tool_test_hint0_" + std::to_string(::rand()));
    fs::create_directories(tmp);
    std::string file = (tmp / "dup0.cpp").string();
    writeFile(file, "return done();\nreturn done();\n");
    ExecuteService svc((tmp / "chk").string());
    EditAction e{file, "return done();", "X", 0};
    auto r = svc.apply({e});
    REQUIRE(r.ok);
    CHECK(readFile(file) == "X\nreturn done();\n");
    fs::remove_all(tmp);
}
