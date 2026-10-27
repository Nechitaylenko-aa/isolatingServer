#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <sqlite3.h>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SProject.h"
#include "index/CClangFactory.h"
#include "index/CSQLiteIndex.h"
#include "index/CSchemaMigrator.h"
#include "index/CSqlCompilerIndex.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_ci_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс имени.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_ci_counter++));
    std::filesystem::create_directories(base);
    return base;
}

/**
 * @brief RAII-переопределение HOME.
 */
class HomeScope
{
public:
    /**
     * @brief Установить HOME.
     * @param home Новый каталог.
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
     * @brief Восстановить HOME.
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
 * @brief Путь к обкаточному проекту (переопределяется FCPP_AGENT_DOGFOOD).
 * @return Путь либо пустая строка.
 */
std::filesystem::path dogfood_path()
{
    const char* env = std::getenv("FCPP_AGENT_DOGFOOD");
    if (env != nullptr && env[0] != '\0')
    {
        return std::filesystem::path(env);
    }
    const std::filesystem::path fallback = "/home/artem/projects/outer/sandbox/dogfood";
    if (std::filesystem::exists(fallback / "build" / "compile_commands.json"))
    {
        return fallback;
    }
    return {};
}

} // namespace

TEST_CASE("store_inventory дедуплицирует TU из нескольких targets", "[compiler_index]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-ci-inventory-dedup");
    const std::filesystem::path project = home / "project";
    const std::filesystem::path source = project / "src" / "CProtocolServer.cpp";
    const std::filesystem::path database = home / "index.db";
    std::filesystem::create_directories(source.parent_path());
    {
        std::ofstream output(source);
        REQUIRE(output.is_open());
        output << "int protocol_server() { return 1; }\\n";
    }

    fcpp::CSchemaMigrator migrator;
    REQUIRE(migrator.migrate(database.string()));

    fcpp::CSQLiteIndex index(database);
    REQUIRE(index.open(project));

    fcpp::SCompileCommand agent_command;
    agent_command.m_file = source;
    agent_command.m_directory = project;
    agent_command.m_arguments = {"-std=c++20", "-DAGENT_TARGET"};
    fcpp::SCompileCommand test_command;
    test_command.m_file = source;
    test_command.m_directory = project;
    test_command.m_arguments = {"-std=c++20", "-DTEST_TARGET"};

    REQUIRE(index.store_inventory({agent_command, test_command}));

    sqlite3* db = index.handle();
    REQUIRE(db != nullptr);
    const auto scalar = [db](const char* sql) -> std::int64_t
    {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
        {
            return -1;
        }
        std::int64_t value = -1;
        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            value = sqlite3_column_int64(statement, 0);
        }
        sqlite3_finalize(statement);
        return value;
    };

    REQUIRE(scalar("SELECT COUNT(*) FROM file WHERE generation = 1;") == 1);
    REQUIRE(scalar("SELECT COUNT(*) FROM compile_command;") == 1);
    REQUIRE(scalar("SELECT file_count FROM index_generation WHERE generation = 1;") == 1);

    std::vector<std::string> stored_arguments;
    REQUIRE(index.load_file_arguments(source.string(), stored_arguments));
    REQUIRE(stored_arguments == agent_command.m_arguments);

    index.close_db();
    std::filesystem::remove_all(home);
}

TEST_CASE("CClang22 доступен и сообщает версию", "[compiler_index]")
{
    std::unique_ptr<fcpp::IClang> clang = fcpp::create_clang_backend({});
    REQUIRE(clang != nullptr);
    REQUIRE(clang->is_available());
    REQUIRE_FALSE(clang->version().empty());
}

TEST_CASE("cursor_at предпочитает метод параметру для контекста вопроса", "[compiler_index]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-ci-cursor-parameter");
    const std::filesystem::path source = home / "cursor.cpp";
    const std::string code = "struct C { void run(int commands) {} };\n";
    {
        std::ofstream output(source);
        REQUIRE(output.is_open());
        output << code;
    }

    const std::size_t offset = code.find("commands");
    REQUIRE(offset != std::string::npos);
    std::unique_ptr<fcpp::IClang> clang = fcpp::create_clang_backend({});
    REQUIRE(clang != nullptr);

    const std::optional<fcpp::SClangSymbol> symbol =
        clang->cursor_at(source, 1, static_cast<std::uint32_t>(offset + 1), {},
                         fcpp::EFocusKind::Innermost);
    REQUIRE(symbol.has_value());
    REQUIRE(symbol->m_kind == fcpp::ESymbolKind::Method);
    REQUIRE(symbol->m_qualified_name == "C::run");

    std::filesystem::remove_all(home);
}

TEST_CASE("open идемпотентен, refresh/rebuild сохраняют одну generation", "[compiler_index]")
{
    const std::filesystem::path dogfood = dogfood_path();
    if (dogfood.empty())
    {
        SUCCEED("dogfood fixture недоступна — пропуск");
        return;
    }

    const std::filesystem::path home = make_temp_dir("fcpp-ci-gen");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(dogfood));
    REQUIRE(manager.open(dogfood));

    REQUIRE(manager.refresh_index());
    REQUIRE(manager.rebuild_index());

    const fcpp::SProjectContext& context = manager.current();
    REQUIRE(context.m_index_status.m_symbol_count > 0);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(context.m_project.m_db_path.string().c_str(), &db) == SQLITE_OK);

    sqlite3_stmt* statement = nullptr;
    std::int64_t generations = -1;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM index_generation;", -1, &statement, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            generations = sqlite3_column_int64(statement, 0);
        }
        sqlite3_finalize(statement);
    }
    REQUIRE(generations == 1);

    sqlite3_close(db);
    std::filesystem::remove_all(home);
}

TEST_CASE("cursor_at находит сущность по позиции", "[compiler_index]")
{
    const std::filesystem::path dogfood = dogfood_path();
    if (dogfood.empty())
    {
        SUCCEED("dogfood fixture недоступна — пропуск");
        return;
    }

    const std::filesystem::path header = dogfood / "src" / "model" / "shape.hpp";
    REQUIRE(std::filesystem::exists(header));

    std::uint32_t area_line = 0;
    {
        std::ifstream input(header);
        std::string line;
        std::uint32_t number = 0;
        while (std::getline(input, line))
        {
            ++number;
            if (line.find("area()") != std::string::npos)
            {
                area_line = number;
                break;
            }
        }
    }
    REQUIRE(area_line > 0);

    std::unique_ptr<fcpp::IClang> clang = fcpp::create_clang_backend({});
    REQUIRE(clang != nullptr);

    const std::vector<std::string> arguments =
    {
        "-x", "c++", "-std=c++20", "-I" + (dogfood / "src").string()
    };

    const std::optional<fcpp::SClangSymbol> symbol = clang->cursor_at(header, area_line, 20, arguments);
    REQUIRE(symbol.has_value());
    REQUIRE(symbol->m_name == "area");
}

TEST_CASE("прогрев dogfood наполняет граф отношений", "[compiler_index]")
{
    const std::filesystem::path dogfood = dogfood_path();
    if (dogfood.empty())
    {
        SUCCEED("dogfood fixture недоступна — пропуск");
        return;
    }

    const std::filesystem::path home = make_temp_dir("fcpp-ci-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(dogfood));

    const fcpp::SProjectContext& context = manager.current();
    REQUIRE(context.m_index_status.m_state == fcpp::EIndexState::Ready);
    REQUIRE(context.m_index_status.m_symbol_count > 0);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(context.m_project.m_db_path.string().c_str(), &db) == SQLITE_OK);

    fcpp::CSqlCompilerIndex index(db);

    // Наследование: Circle/Rectangle наследуют Shape.
    const std::vector<std::string> shape_derived = [&db, &index]()
    {
        std::vector<std::string> result;
        sqlite3_stmt* statement = nullptr;
        const char* sql = "SELECT usr FROM symbol WHERE qualified_name = 'dogfood::model::Shape' AND kind = 'class' LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(statement) == SQLITE_ROW)
            {
                const std::string usr = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
                const std::vector<fcpp::SBaseType> derived = index.derived_types(usr);
                for (const fcpp::SBaseType& entry : derived)
                {
                    result.push_back(entry.m_usr);
                }
            }
            sqlite3_finalize(statement);
        }
        return result;
    }();
    REQUIRE(shape_derived.size() == 2);

    // Диспетчеризация: area() базового Shape резолвится до реализаций.
    const std::vector<std::string> dispatch = [&db, &index]()
    {
        std::vector<std::string> result;
        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "SELECT usr FROM symbol WHERE qualified_name = 'dogfood::model::Shape::area' LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(statement) == SQLITE_ROW)
            {
                const std::string usr = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
                result = index.dispatch_targets(usr);
            }
            sqlite3_finalize(statement);
        }
        return result;
    }();
    REQUIRE(dispatch.size() == 3);

    sqlite3_close(db);
    std::filesystem::remove_all(home);
}

TEST_CASE("чтение индекса: outline/callers/callees/refs/source", "[compiler_index]")
{
    const std::filesystem::path dogfood = dogfood_path();
    if (dogfood.empty())
    {
        SUCCEED("dogfood fixture недоступна — пропуск");
        return;
    }

    const std::filesystem::path home = make_temp_dir("fcpp-ci-read");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(dogfood));

    const fcpp::SProjectContext& context = manager.current();

    fcpp::CSQLiteIndex index(context.m_project.m_db_path);
    REQUIRE(index.open_db());

    const std::vector<fcpp::SSymbol> shapes = index.find_symbols_by_name("Shape");
    REQUIRE_FALSE(shapes.empty());

    const std::vector<fcpp::SSymbol> areas = index.find_symbols_by_name("area");
    REQUIRE(areas.size() >= 3);

    const std::vector<fcpp::SSymbol> shape_members = index.outline(shapes.front().m_usr);
    REQUIRE(shape_members.size() >= 2);

    bool any_callers = false;
    for (const fcpp::SSymbol& area : areas)
    {
        if (!index.callers(area.m_usr).empty())
        {
            any_callers = true;
            break;
        }
    }
    REQUIRE(any_callers);

    bool any_refs = false;
    for (const fcpp::SSymbol& area : areas)
    {
        if (!index.references(area.m_usr).empty())
        {
            any_refs = true;
            break;
        }
    }
    REQUIRE(any_refs);

    const std::optional<fcpp::SSourceSlice> slice = index.get_source_slice(areas.front().m_usr);
    REQUIRE(slice.has_value());
    REQUIRE_FALSE(slice->m_text.empty());
    REQUIRE(slice->m_range.m_line > 0);
    REQUIRE(slice->m_range.m_column > 0);
    REQUIRE(slice->m_range.m_end_line >= slice->m_range.m_line);
    REQUIRE(slice->m_range.m_end_column > 0);

    // source() must begin at libclang's exact byte column, not at the line start.
    std::filesystem::path source_path = areas.front().m_location.m_file;
    if (source_path.is_relative())
    {
        source_path = dogfood / source_path;
    }
    std::ifstream source_file(source_path);
    REQUIRE(source_file.is_open());
    std::string source_line;
    for (std::uint32_t line_number = 1; line_number <= slice->m_range.m_line; ++line_number)
    {
        REQUIRE(static_cast<bool>(std::getline(source_file, source_line)));
    }
    const std::size_t start_offset = static_cast<std::size_t>(slice->m_range.m_column - 1);
    REQUIRE(start_offset <= source_line.size());
    const std::size_t first_line_length =
        (slice->m_range.m_line == slice->m_range.m_end_line)
            ? static_cast<std::size_t>(slice->m_range.m_end_column - slice->m_range.m_column)
            : std::string::npos;
    REQUIRE(slice->m_text.starts_with(source_line.substr(start_offset, first_line_length) + '\n'));

    index.close_db();
    std::filesystem::remove_all(home);
}
