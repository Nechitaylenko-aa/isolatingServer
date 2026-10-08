#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SProject.h"
#include "index/CSQLiteIndex.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_stale_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_stale_counter++));
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
 * @brief Создать минимальный проект с двумя TU, включающими общий заголовок.
 * @return Путь к проекту.
 */
std::filesystem::path make_project()
{
    const std::filesystem::path project = make_temp_dir("fcpp-stale-proj");
    const std::filesystem::path src = project / "src";
    std::filesystem::create_directories(src);
    std::filesystem::create_directories(project / "build");

    std::ofstream(project / "CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(stale LANGUAGES CXX)\n";

    std::ofstream(src / "a.hpp")
        << "#pragma once\nnamespace stale { struct Widget { int value() const; }; }\n";
    std::ofstream(src / "a.cpp")
        << "#include \"a.hpp\"\nnamespace stale { int Widget::value() const { return 1; } }\n";
    std::ofstream(src / "b.cpp")
        << "#include \"a.hpp\"\nnamespace stale { int use() { Widget w; return w.value(); } }\n";

    const std::string directory = project.string();
    const std::string include_flag = "-I" + src.string();

    std::ofstream commands(project / "build" / "compile_commands.json");
    commands << "[\n";
    commands << "  {\"directory\":\"" << directory << "\",\"file\":\"" << (src / "a.cpp").string()
             << "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"" << include_flag << "\",\"-c\",\""
             << (src / "a.cpp").string() << "\"]},\n";
    commands << "  {\"directory\":\"" << directory << "\",\"file\":\"" << (src / "b.cpp").string()
             << "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"" << include_flag << "\",\"-c\",\""
             << (src / "b.cpp").string() << "\"]}\n";
    commands << "]\n";

    return project;
}

} // namespace

TEST_CASE("stale: изменение заголовка помечает зависимые TU", "[stale]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-stale-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_project();

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(project_dir));

    const fcpp::SProjectContext& project = manager.current();
    REQUIRE(project.m_index_status.m_symbol_count > 0);

    // Актуальный индекс не должен помечаться устаревшим.
    {
        fcpp::CSQLiteIndex index(project.m_project.m_db_path);
        REQUIRE(index.open(project.m_project.m_canonical_path));
        fcpp::SStaleReport report;
        REQUIRE(index.detect_stale((project_dir / "build" / "compile_commands.json").string(), report));
        REQUIRE_FALSE(report.is_stale());
        index.close_db();
    }

    // Изменяем заголовок (меняем размер файла для надёжной детекции).
    {
        std::ofstream header(project_dir / "src" / "a.hpp", std::ios::app);
        header << "// changed marker to force size delta\n";
    }

    // Детекция: должны быть помечены оба зависимых TU.
    {
        fcpp::CSQLiteIndex index(project.m_project.m_db_path);
        REQUIRE(index.open(project.m_project.m_canonical_path));
        fcpp::SStaleReport report;
        REQUIRE(index.detect_stale((project_dir / "build" / "compile_commands.json").string(), report));
        REQUIRE(report.is_stale());
        REQUIRE(report.m_reason == fcpp::EStaleReason::ContentChange);
        REQUIRE(report.m_dirty_tus.size() == 2);
        index.close_db();
    }

    // Точечный refresh.
    REQUIRE(manager.refresh_index());
    REQUIRE(manager.current().m_index_status.m_symbol_count > 0);

    // После refresh индекс снова актуален.
    {
        fcpp::CSQLiteIndex index(project.m_project.m_db_path);
        REQUIRE(index.open(project.m_project.m_canonical_path));
        fcpp::SStaleReport report;
        REQUIRE(index.detect_stale((project_dir / "build" / "compile_commands.json").string(), report));
        REQUIRE_FALSE(report.is_stale());
        index.close_db();
    }

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}
