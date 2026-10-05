#include <catch2/catch_test_macros.hpp>
#include "config_reader.h"

#include <filesystem>
#include <fstream>

using namespace cppagent;
namespace fs = std::filesystem;

namespace {
struct TempConf {
    fs::path dir;
    TempConf() {
        dir = fs::temp_directory_path() / ("cf_agent_conf_" + std::to_string(::rand()));
        fs::create_directories(dir);
    }
    ~TempConf() { std::error_code ec; fs::remove_all(dir, ec); }
    void write(const std::string& name, const std::string& content) {
        std::ofstream out(dir / name, std::ios::trunc);
        out << content;
    }
};
} // namespace

TEST_CASE("CConfigReader: parses agent.conf-style key/value with comments", "[config]") {
    std::string k, v;
    CHECK(CConfigReader::parseKeyValue("model_main=\"http://x:8080\"", k, v));
    CHECK(k == "model_main");
    CHECK(v == "http://x:8080");
    CHECK(CConfigReader::parseKeyValue("  db_dir=~/.cf_agent/db  ", k, v));
    CHECK(k == "db_dir");
    CHECK(v == "~/.cf_agent/db");
    CHECK(!CConfigReader::parseKeyValue("# comment", k, v));
    CHECK(!CConfigReader::parseKeyValue("", k, v));
    CHECK(!CConfigReader::parseKeyValue("noequals", k, v));
}

TEST_CASE("CConfigReader: light aliases to main when same_as_main", "[config]") {
    TempConf c;
    c.write(kConfigFileName,
            "# mine\n"
            "model_main=\"http://127.0.0.1:8080\"\n"
            "model_main_name=\"qwen.gguf\"\n"
            "model_light=\"same_as_main\"\n"
            "db_dir=\"" + c.dir.string() + "/db\"\n"
            "default_build=\"build\"\n");
    auto cfg = CConfigReader::loadOrCreate(c.dir.string());
    CHECK(cfg.config().main.url == "http://127.0.0.1:8080");
    CHECK(cfg.config().light.url == "http://127.0.0.1:8080"); // alias
    CHECK(cfg.config().light.name == "qwen.gguf");            // унаследовано
    CHECK(cfg.config().defaultBuild == "build");
}

TEST_CASE("CConfigReader: access policies default and override", "[config]") {
    TempConf c;
    c.write(kConfigFileName, "db_dir=\"" + c.dir.string() + "/db\"\n");
    auto cfg = CConfigReader::loadOrCreate(c.dir.string());
    // дефолты: read/write=YES, exec/subtasks=NO
    CHECK(cfg.config().allowRead);
    CHECK(cfg.config().allowWrite);
    CHECK(!cfg.config().allowExec);
    CHECK(!cfg.config().allowSubtasks);

    c.write(kConfigFileName,
            "allow_read=\"NO\"\nallow_write=\"NO\"\nallow_exec=\"YES\"\nallow_subtasks=\"YES\"\n"
            "db_dir=\"" + c.dir.string() + "/db\"\n");
    auto cfg2 = CConfigReader::loadOrCreate(c.dir.string());
    CHECK(!cfg2.config().allowRead);
    CHECK(!cfg2.config().allowWrite);
    CHECK(cfg2.config().allowExec);
    CHECK(cfg2.config().allowSubtasks);
}

TEST_CASE("CConfigReader: model getters and refresh_from_config", "[config]") {
    TempConf c;
    c.write(kConfigFileName,
            "model_main=\"http://m:1\"\nmodel_main_name=\"qwen-main\"\n"
            "model_light=\"same_as_main\"\n"
            "model_remote_url=\"https://r\"\nmodel_remote_name=\"claude\"\n"
            "db_dir=\"" + c.dir.string() + "/db\"\n");
    auto cfg = CConfigReader::loadOrCreate(c.dir.string());
    CHECK(cfg.get_main_model() == "qwen-main @ http://m:1");
    CHECK(cfg.get_remote_model() == "claude @ https://r");

    // правим конфиг и перечитываем
    c.write(kConfigFileName,
            "model_main=\"http://m:2\"\nmodel_main_name=\"qwen2\"\n"
            "db_dir=\"" + c.dir.string() + "/db\"\n");
    REQUIRE(cfg.refresh_from_config());
    CHECK(cfg.get_main_model() == "qwen2 @ http://m:2");
    CHECK(cfg.get_remote_model().empty()); // remote убран в новом конфиге
}

TEST_CASE("CConfigReader: parses remote endpoint (type/url/name/key)", "[config]") {
    TempConf c;
    c.write(kConfigFileName,
            "model_remote_url=\"https://api.anthropic.com\"\n"
            "model_remote_type=\"claude\"\n"
            "model_remote_name=\"claude-3-5-sonnet\"\n"
            "model_remote_key=\"sk-test\"\n"
            "db_dir=\"" + c.dir.string() + "/db\"\n");
    auto cfg = CConfigReader::loadOrCreate(c.dir.string());
    CHECK(cfg.config().remote.url == "https://api.anthropic.com");
    CHECK(cfg.config().remote.apiType == "claude");
    CHECK(cfg.config().remote.name == "claude-3-5-sonnet");
    CHECK(cfg.config().remote.apiKey == "sk-test");
}

TEST_CASE("CConfigReader: creates default agent.conf and projects when missing", "[config]") {
    fs::path dir = fs::temp_directory_path() / ("cf_agent_new_" + std::to_string(::rand()));
    std::error_code ec; fs::remove_all(dir, ec);
    auto cfg = CConfigReader::loadOrCreate(dir.string());
    CHECK(fs::exists(dir / kConfigFileName));
    CHECK(fs::exists(dir / "projects"));
    CHECK(cfg.config().main.configured()); // дефолт http://127.0.0.1:8080
    fs::remove_all(dir, ec);
}

TEST_CASE("CConfigReader: resolve --cc keeps build=parent, db deterministic", "[config]") {
    TempConf c;
    fs::path proj = c.dir / "proj";
    fs::create_directories(proj / "build");
    std::ofstream(proj / "build" / "compile_commands.json") << "[]";
    c.write(kConfigFileName, "db_dir=\"" + c.dir.string() + "/db\"\n");

    auto cfg = CConfigReader::loadOrCreate(c.dir.string());
    auto r = cfg.resolve((proj / "build" / "compile_commands.json").string(), "", "");
    REQUIRE(r.ok);
    CHECK(r.buildDir == fs::weakly_canonical(proj / "build").string());
    CHECK(r.dbFile.find("/db/") != std::string::npos);
    CHECK(r.dbFile.size() > 4);
    CHECK(r.dbFile.substr(r.dbFile.size() - 3) == ".db");
}

TEST_CASE("CConfigReader: resolve --project auto-registers and maps db", "[config]") {
    TempConf c;
    fs::path proj = c.dir / "myproj";
    fs::create_directories(proj);
    c.write(kConfigFileName, "db_dir=\"" + c.dir.string() + "/db\"\n"
                       "default_build=\"build\"\n");
    auto cfg = CConfigReader::loadOrCreate(c.dir.string());

    auto r = cfg.resolve("", proj.string(), "");
    REQUIRE(r.ok);
    CHECK(r.buildDir == fs::weakly_canonical(proj / "build").string());
    CHECK(r.compileCommands == r.buildDir + "/compile_commands.json");
    // авто-регистрация попала в projects
    std::ifstream in(c.dir / "projects");
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(all.find(fs::weakly_canonical(proj).string()) != std::string::npos);
}

TEST_CASE("CConfigReader: db_file override from projects wins", "[config]") {
    TempConf c;
    fs::path proj = c.dir / "p";
    fs::create_directories(proj);
    std::string customDb = (c.dir / "custom.db").string();
    c.write(kConfigFileName, "db_dir=\"" + c.dir.string() + "/db\"\n");
    c.write("projects", fs::weakly_canonical(proj).string() + " out/debug " + customDb + "\n");
    auto cfg = CConfigReader::loadOrCreate(c.dir.string());
    auto r = cfg.resolve("", proj.string(), "");
    REQUIRE(r.ok);
    CHECK(r.buildDir == fs::weakly_canonical(proj / "out/debug").string());
    CHECK(r.dbFile == customDb);
}
