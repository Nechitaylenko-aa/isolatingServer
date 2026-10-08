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

TEST_CASE("CClang22 доступен и сообщает версию", "[compiler_index]")
{
    std::unique_ptr<fcpp::IClang> clang = fcpp::create_clang_backend({});
    REQUIRE(clang != nullptr);
    REQUIRE(clang->is_available());
    REQUIRE_FALSE(clang->version().empty());
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

    index.close_db();
    std::filesystem::remove_all(home);
}
