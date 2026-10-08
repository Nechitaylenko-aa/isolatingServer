#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "core/CConfigManager.h"
#include "core/CDoctor.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SProject.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_s8_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_s8_counter++));
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
 * @brief Создать минимальный проект.
 * @return Путь к проекту.
 */
std::filesystem::path make_project()
{
    const std::filesystem::path project = make_temp_dir("fcpp-s8-proj");
    std::filesystem::create_directories(project / "src");
    std::filesystem::create_directories(project / "build");
    std::ofstream(project / "CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(s8 LANGUAGES CXX)\n";
    std::ofstream(project / "src" / "main.cpp") << "int main() { return 0; }\n";

    std::ofstream commands(project / "build" / "compile_commands.json");
    commands << "[{\"directory\":\"" << project.string() << "\",\"file\":\""
             << (project / "src" / "main.cpp").string() << "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"-c\",\""
             << (project / "src" / "main.cpp").string() << "\"]}]\n";
    return project;
}

} // namespace

TEST_CASE("config: allow_* читаются и имеют безопасные дефолты", "[stage8a]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-s8-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    // Дефолты bootstrap: read/write/exec = YES, subtasks = NO.
    REQUIRE(config.config().m_allow_read);
    REQUIRE(config.config().m_allow_write);
    REQUIRE(config.config().m_allow_exec);
    REQUIRE_FALSE(config.config().m_allow_subtasks);

    // Перезапись: запрещаем запись.
    {
        std::ofstream conf(config.config_path());
        conf << "enable_tidy=\"NO\"\n";
        conf << "registry_path=\"" << (home / ".fcpp-agent" / "projects" / "registry.db").string() << "\"\n";
        conf << "allow_read=\"YES\"\n";
        conf << "allow_write=\"NO\"\n";
        conf << "allow_exec=\"NO\"\n";
        conf << "allow_subtasks=\"YES\"\n";
    }

    fcpp::CConfigManager reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.config().m_allow_read);
    REQUIRE_FALSE(reloaded.config().m_allow_write);
    REQUIRE_FALSE(reloaded.config().m_allow_exec);
    REQUIRE(reloaded.config().m_allow_subtasks);

    std::filesystem::remove_all(home);
}

TEST_CASE("session: текущий проект сохраняется и восстанавливается", "[stage8a]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-s8-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_project();

    {
        fcpp::CProjectManager manager(config);
        REQUIRE(manager.open(project_dir));
        REQUIRE(manager.save_session());
    }

    // Новый "процесс" (новый менеджер) восстанавливает сессию.
    {
        fcpp::CProjectManager manager(config);
        REQUIRE_FALSE(manager.has_current());
        REQUIRE(manager.restore_session());
        REQUIRE(manager.has_current());
        REQUIRE(manager.current().m_project.m_canonical_path == std::filesystem::canonical(project_dir));

        manager.clear_session();
    }

    {
        fcpp::CProjectManager manager(config);
        REQUIRE(manager.restore_session());
        REQUIRE_FALSE(manager.has_current());
    }

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}

TEST_CASE("config: модели, db_dir, default_build читаются", "[stage8a]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-s8-cfg");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());

    // Перезапись кастомными значениями.
    {
        std::ofstream conf(config.config_path());
        conf << "enable_tidy=\"NO\"\n";
        conf << "registry_path=\"" << (home / ".fcpp-agent" / "reg.db").string() << "\"\n";
        conf << "db_dir=\"/tmp/custom-db\"\n";
        conf << "default_build=\"tmp/build\"\n";
        conf << "model_main=\"http://127.0.0.1:8080\"\n";
        conf << "model_main_name=\"deepseek\"\n";
        conf << "model_light=\"same_as_main\"\n";
        conf << "model_remote_url=\"https://routerai.ru/api/v1\"\n";
        conf << "model_remote_type=\"claude\"\n";
        conf << "model_remote_name=\"claude-3.5\"\n";
        conf << "model_remote_key=\"sk-secret\"\n";
    }

    fcpp::CConfigManager reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.config().m_default_build == "tmp/build");
    REQUIRE(reloaded.config().m_db_dir == std::filesystem::path("/tmp/custom-db"));
    REQUIRE(reloaded.config().m_model_main.m_name == "deepseek");
    REQUIRE(reloaded.config().m_model_light.m_url == "same_as_main");
    REQUIRE(reloaded.config().m_remote_type == fcpp::EModelRemoteType::Claude);
    REQUIRE(reloaded.config().m_model_remote.m_key == "sk-secret");

    std::filesystem::remove_all(home);
}

TEST_CASE("reload_config подхватывает изменения на лету", "[stage8a]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-s8-reload");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());
    const std::string original_build = config.config().m_default_build;

    // Меняем значение в файле и перечитываем без пересоздания менеджера.
    {
        std::ofstream conf(config.config_path(), std::ios::app);
        conf << "default_build=\"reloaded-build\"\n";
    }

    REQUIRE(config.reload_config());
    REQUIRE(config.config().m_default_build == "reloaded-build");
    REQUIRE(config.config().m_default_build != original_build);

    std::filesystem::remove_all(home);
}

TEST_CASE("doctor: критичные инструменты доступны", "[stage8a]")
{
    fcpp::SConfig config;
    REQUIRE(fcpp::CDoctor().run(config));
}
