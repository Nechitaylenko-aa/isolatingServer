#include <catch2/catch_test_macros.hpp>
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
