#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include <sqlite3.h>

#include "core/CCompileCommandsLocator.h"
#include "core/CCompileCommandsParser.h"
#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SProject.h"

namespace
{

/** @brief Счётчик для уникальных имён временных каталогов. */
int g_cc_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс имени каталога.
 * @return Путь к созданному каталогу.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_cc_counter++));
    std::filesystem::create_directories(base);
    return base;
}

/**
 * @brief Записать минимальный CMakeLists.txt.
 * @param dir Каталог проекта.
 */
void write_cmake_lists(const std::filesystem::path& dir)
{
    std::ofstream output(dir / "CMakeLists.txt");
    output << "cmake_minimum_required(VERSION 3.20)\n";
    output << "project(fixture LANGUAGES CXX)\n";
}

/**
 * @brief Записать compile_commands.json в стандартный build-каталог.
 * @param project_dir Каталог проекта.
 * @param source_name Имя исходного файла.
 */
void write_compile_commands(const std::filesystem::path& project_dir, const std::string& source_name)
{
    const std::filesystem::path build_dir = project_dir / "build";
    std::filesystem::create_directories(build_dir);

    std::ofstream(source_name.empty() ? (project_dir / "dummy") : (project_dir / source_name))
        << "int main() { return 0; }\n";

    std::ofstream output(build_dir / "compile_commands.json");
    output << "[\n";
    output << "  {\n";
    output << "    \"directory\": \"" << project_dir.string() << "\",\n";
    output << "    \"file\": \"" << source_name << "\",\n";
    output << "    \"command\": \"/usr/bin/clang++ -std=c++20 -c " << source_name << "\"\n";
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
     * @brief Установить HOME.
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

/**
 * @brief Подсчитать строки в таблице базы данных.
 * @param db_path Путь к базе.
 * @param table Имя таблицы.
 * @return Число строк (0 при ошибке).
 */
std::int64_t count_rows(const std::filesystem::path& db_path, const std::string& table)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK)
    {
        return -1;
    }

    const std::string query = "SELECT COUNT(*) FROM " + table + ";";
    sqlite3_stmt* statement = nullptr;
    std::int64_t count = -1;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &statement, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            count = sqlite3_column_int64(statement, 0);
        }
    }
    sqlite3_finalize(statement);
    sqlite3_close(db);
    return count;
}

} // namespace

TEST_CASE("locator предпочитает явный путь", "[compile_commands]")
{
    const std::filesystem::path project_dir = make_temp_dir("fcpp-cc-locate");
    write_cmake_lists(project_dir);
    write_compile_commands(project_dir, "main.cpp");

    const std::filesystem::path explicit_path = project_dir / "custom.json";
    std::ofstream(explicit_path) << "[]\n";

    const fcpp::SCompileCommandsLocation location =
        fcpp::CCompileCommandsLocator::resolve(project_dir, explicit_path, {}, "build");
    REQUIRE(location.m_found);
    REQUIRE(location.m_path == explicit_path);

    std::filesystem::remove_all(project_dir);
}

TEST_CASE("locator возвращает not found без источников", "[compile_commands]")
{
    const std::filesystem::path project_dir = make_temp_dir("fcpp-cc-miss");
    write_cmake_lists(project_dir);

    const fcpp::SCompileCommandsLocation location =
        fcpp::CCompileCommandsLocator::resolve(project_dir, {}, {}, "build");
    REQUIRE_FALSE(location.m_found);

    std::filesystem::remove_all(project_dir);
}

TEST_CASE("parser отклоняет невалидный JSON", "[compile_commands]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-cc-badjson");
    const std::filesystem::path bad = dir / "compile_commands.json";
    std::ofstream(bad) << "{ not an array }";

    std::vector<fcpp::SCompileCommand> commands;
    REQUIRE_FALSE(fcpp::CCompileCommandsParser::parse(bad, commands));

    std::filesystem::remove_all(dir);
}

TEST_CASE("open наполняет инвентарь TU", "[compile_commands]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_temp_dir("fcpp-cc-open");
    write_cmake_lists(project_dir);
    write_compile_commands(project_dir, "main.cpp");

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(project_dir));

    const fcpp::SProjectContext& context = manager.current();
    REQUIRE(context.m_index_status.m_state == fcpp::EIndexState::Ready);
    REQUIRE(context.m_index_status.m_file_count == 1);
    REQUIRE(count_rows(context.m_project.m_db_path, "compile_command") == 1);

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}

TEST_CASE("open без compile_commands завершается FAIL без следов", "[compile_commands]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_temp_dir("fcpp-cc-fail");
    write_cmake_lists(project_dir);

    fcpp::CProjectManager manager(config);
    REQUIRE_FALSE(manager.open(project_dir));
    REQUIRE_FALSE(manager.has_current());
    REQUIRE(manager.list().empty());

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}


TEST_CASE("compile command parser honors escaped spaces and rejects unclosed quotes", "[compile_commands]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-cc-escaped");
    const std::filesystem::path path = dir / "compile_commands.json";
    std::ofstream(path) << R"([{"directory":"/tmp","file":"main.cpp","command":"clang++ -Iinclude\\ dir -c main.cpp"}])";

    std::vector<fcpp::SCompileCommand> commands;
    REQUIRE(fcpp::CCompileCommandsParser::parse(path, commands));
    REQUIRE(commands.size() == 1);
    REQUIRE(commands[0].m_arguments.size() == 4);
    REQUIRE(commands[0].m_arguments[1] == "-Iinclude dir");

    std::ofstream(path) << R"([{"directory":"/tmp","file":"main.cpp","command":"clang++ \"unfinished"}])";
    REQUIRE_FALSE(fcpp::CCompileCommandsParser::parse(path, commands));
    REQUIRE(commands.empty());

    std::filesystem::remove_all(dir);
}


TEST_CASE("compile command parser rejects empty command entries", "[compile_commands]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-cc-empty-command");
    const std::filesystem::path path = dir / "compile_commands.json";
    std::vector<fcpp::SCompileCommand> commands;

    std::ofstream(path) << R"([{"directory":"/tmp","file":"main.cpp","command":""}])";
    REQUIRE_FALSE(fcpp::CCompileCommandsParser::parse(path, commands));
    REQUIRE(commands.empty());

    std::ofstream(path) << R"([{"directory":"/tmp","file":"main.cpp","command":"   "}])";
    REQUIRE_FALSE(fcpp::CCompileCommandsParser::parse(path, commands));
    REQUIRE(commands.empty());

    std::filesystem::remove_all(dir);
}
