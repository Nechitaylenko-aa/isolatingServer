#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "core/CConfigManager.h"
#include "core/CHash.h"
#include "core/CProjectManager.h"
#include "core/CProjectPaths.h"
#include "fcpp-agent/SProject.h"
#include "index/CSchemaInitializer.h"

#include <sqlite3.h>

namespace
{

/** @brief Счётчик для уникальных имён временных каталогов. */
int g_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс имени каталога.
 * @return Путь к созданному каталогу.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_counter++));
    std::filesystem::create_directories(base);
    return base;
}

/**
 * @brief Записать минимальный CMakeLists.txt в каталог.
 * @param dir Каталог проекта.
 */
void write_cmake_lists(const std::filesystem::path& dir)
{
    std::ofstream output(dir / "CMakeLists.txt");
    output << "cmake_minimum_required(VERSION 3.20)\n";
    output << "project(fixture LANGUAGES CXX)\n";
}

/**
 * @brief Записать compile_commands.json и исходник в стандартный build-каталог.
 * @param dir Каталог проекта.
 */
void write_compile_commands(const std::filesystem::path& dir)
{
    std::ofstream(dir / "main.cpp") << "int main() { return 0; }\n";

    const std::filesystem::path build_dir = dir / "build";
    std::filesystem::create_directories(build_dir);

    std::ofstream output(build_dir / "compile_commands.json");
    output << "[\n";
    output << "  {\n";
    output << "    \"directory\": \"" << dir.string() << "\",\n";
    output << "    \"file\": \"main.cpp\",\n";
    output << "    \"command\": \"/usr/bin/clang++ -std=c++20 -c main.cpp\"\n";
    output << "  }\n";
    output << "]\n";
}

/**
 * @brief RAII-переопределение HOME на время теста.
 */
class HomeScope
{
public:
    /**
     * @brief Установить HOME в заданный каталог.
     * @param home Новый домашний каталог.
     */
    explicit HomeScope(const std::filesystem::path& home)
    {
        const char* previous = std::getenv("HOME");
        if (previous != nullptr)
        {
            m_previous = previous;
        }
        setenv("HOME", home.string().c_str(), 1);
    }

    /**
     * @brief Восстановить прежний HOME.
     */
    ~HomeScope()
    {
        if (m_previous.empty())
        {
            unsetenv("HOME");
        }
        else
        {
            setenv("HOME", m_previous.c_str(), 1);
        }
    }

    HomeScope(const HomeScope&) = delete;
    HomeScope& operator=(const HomeScope&) = delete;

private:
    std::string m_previous;
};

} // namespace

TEST_CASE("incompatible existing database is rejected without schema repair", "[project][schema]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    const std::filesystem::path db_path = home / "incompatible.db";

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(db_path.string().c_str(), &db) == SQLITE_OK);
    REQUIRE(sqlite3_exec(db,
                         "CREATE TABLE project_profile (id INTEGER PRIMARY KEY);",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    REQUIRE_FALSE(fcpp::CSchemaInitializer::initialize_or_validate(db_path));

    REQUIRE(sqlite3_open_v2(db_path.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(db,
                               "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%';",
                               -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
    REQUIRE(sqlite3_column_int(statement, 0) == 1);
    sqlite3_finalize(statement);
    REQUIRE(sqlite3_close(db) == SQLITE_OK);

    std::filesystem::remove_all(home);
}

TEST_CASE("bootstrap создаёт пустой реестр проектов", "[project]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.list().empty());

    std::filesystem::remove_all(home);
}

TEST_CASE("open регистрирует проект и создаёт индекс", "[project]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_temp_dir("fcpp-proj");
    write_cmake_lists(project_dir);
    write_compile_commands(project_dir);

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(project_dir));
    REQUIRE(manager.has_current());

    const fcpp::SProjectContext& context = manager.current();
    REQUIRE(manager.resolve_path("src/../main.cpp") == (project_dir / "main.cpp").lexically_normal());
    REQUIRE(manager.resolve_path(project_dir / "src" / ".." / "main.cpp")
            == (project_dir / "main.cpp").lexically_normal());
    REQUIRE(context.m_project.m_project_id.rfind("project-", 0) == 0);
    REQUIRE(std::filesystem::exists(context.m_project.m_db_path));
    REQUIRE(manager.list().size() == 1);

    // Existing databases are validated without changing their schema.
    REQUIRE(fcpp::CSchemaInitializer::initialize_or_validate(context.m_project.m_db_path));

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}

TEST_CASE("повторный open идемпотентен", "[project]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_temp_dir("fcpp-proj");
    write_cmake_lists(project_dir);
    write_compile_commands(project_dir);

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(project_dir));
    const std::string first_id = manager.current().m_project.m_project_id;

    REQUIRE(manager.open(project_dir));
    const std::string second_id = manager.current().m_project.m_project_id;

    REQUIRE(first_id == second_id);
    REQUIRE(manager.list().size() == 1);

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}

TEST_CASE("open без CMakeLists.txt завершается FAIL", "[project]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path not_a_project = make_temp_dir("fcpp-nocmake");

    fcpp::CProjectManager manager(config);
    REQUIRE_FALSE(manager.open(not_a_project));
    REQUIRE_FALSE(manager.has_current());
    REQUIRE(manager.list().empty());

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(not_a_project);
}

TEST_CASE("паспорт проекта переживает переключение и восстановление контекста", "[project][profile]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_a = make_temp_dir("fcpp-profile-a");
    write_cmake_lists(project_a);
    write_compile_commands(project_a);
    const std::filesystem::path project_b = make_temp_dir("fcpp-profile-b");
    write_cmake_lists(project_b);
    write_compile_commands(project_b);
    const std::filesystem::path not_a_project = make_temp_dir("fcpp-profile-invalid");

    std::string project_a_id;
    {
        fcpp::CProjectManager manager(config);
        REQUIRE(manager.open(project_a));
        project_a_id = manager.current().m_project.m_project_id;
        REQUIRE(manager.set_profile("Rules for project A"));
        REQUIRE(manager.profile().get() == "Rules for project A");

        REQUIRE(manager.open(project_b));
        REQUIRE(manager.profile().get().empty());
        REQUIRE(manager.set_profile("Rules for project B"));

        REQUIRE(manager.open(project_a));
        REQUIRE(manager.current().m_project.m_project_id == project_a_id);
        REQUIRE(manager.profile().get() == "Rules for project A");

        // A failed open must not replace the active project or its passport.
        REQUIRE_FALSE(manager.open(not_a_project));
        REQUIRE(manager.current().m_project.m_project_id == project_a_id);
        REQUIRE(manager.profile().get() == "Rules for project A");
    }

    // A new manager simulates process/context reconstruction from project storage.
    {
        fcpp::CProjectManager manager(config);
        REQUIRE(manager.open(project_a));
        REQUIRE(manager.profile().get() == "Rules for project A");
        REQUIRE(manager.open(project_b));
        REQUIRE(manager.profile().get() == "Rules for project B");
    }

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_a);
    std::filesystem::remove_all(project_b);
    std::filesystem::remove_all(not_a_project);
}

TEST_CASE("light restore не считается полноценным открытием проекта", "[project][session]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home-light");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_temp_dir("fcpp-proj-light");
    write_cmake_lists(project_dir);
    write_compile_commands(project_dir);
    {
        fcpp::CProjectManager manager(config);
        REQUIRE(manager.open(project_dir));
        REQUIRE(manager.is_opened());
        REQUIRE(manager.save_session());
    }

    {
        fcpp::CProjectManager manager(config);
        REQUIRE(manager.restore_session_light());
        REQUIRE(manager.has_current());
        REQUIRE_FALSE(manager.is_opened());
        REQUIRE(manager.resolve_path("main.cpp").empty());

        REQUIRE(manager.restore_session());
        REQUIRE(manager.has_current());
        REQUIRE(manager.is_opened());
        REQUIRE(manager.resolve_path("main.cpp").is_absolute());
    }

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}

TEST_CASE("project paths resolve absolute and relative inputs without cwd", "[project][paths]")
{
    const std::filesystem::path base = std::filesystem::temp_directory_path() / "fcpp-path-base";
    REQUIRE(fcpp::CProjectPaths::resolve("src/../main.cpp", base)
            == (base / "main.cpp").lexically_normal());
    REQUIRE(fcpp::CProjectPaths::resolve(base / "src" / ".." / "main.cpp", {})
            == (base / "main.cpp").lexically_normal());
    REQUIRE(fcpp::CProjectPaths::resolve("main.cpp", {}).empty());
}

TEST_CASE("sha256 детерминирован", "[hash]")
{
    REQUIRE(fcpp::CHash::sha256_hex("abc")
            == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(fcpp::CHash::sha256_hex("").size() == 64);
}
