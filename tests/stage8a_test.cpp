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

    // Дефолты bootstrap: read/write/exec = YES.
    REQUIRE(config.config().m_allow_read);
    REQUIRE(config.config().m_allow_write);
    REQUIRE(config.config().m_allow_exec);

    // Перезапись: запрещаем запись.
    {
        std::ofstream conf(config.config_path());
        conf << "enable_tidy=\"NO\"\n";
        conf << "registry_path=\"" << (home / ".fcpp-agent" / "projects" / "registry.db").string() << "\"\n";
        conf << "allow_read=\"YES\"\n";
        conf << "allow_write=\"NO\"\n";
        conf << "allow_exec=\"NO\"\n";
    }

    fcpp::CConfigManager reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.config().m_allow_read);
    REQUIRE_FALSE(reloaded.config().m_allow_write);
    REQUIRE_FALSE(reloaded.config().m_allow_exec);

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
        conf << "llm_coder_smart_api=\"https://routerai.ru/api/v1\"\n";
        conf << "llm_coder_smart_model=\"claude-3.5\"\n";
        conf << "llm_coder_smart_key=\"sk-secret\"\n";
        conf << "llm_coder_smart_type=\"claude\"\n";
        conf << "llm_coder_smart_context_window_tokens=\"200000\"\n";
        conf << "llm_coder_smart_output_reserve_tokens=\"8192\"\n";
        conf << "llm_coder_local_api=\"http://127.0.0.1:8080/v1\"\n";
        conf << "llm_coder_local_model=\"deepseek\"\n";
        conf << "llm_coder_local_context_window_tokens=\"32768\"\n";
        conf << "llm_coder_local_output_reserve_tokens=\"2048\"\n";
        conf << "llm_thinker_smart_alias=\"CODER_SMART\"\n";
        conf << "llm_thinker_local_alias=\"CODER_LOCAL\"\n";
    }

    fcpp::CConfigManager reloaded;
    REQUIRE(reloaded.load());
    REQUIRE(reloaded.config().m_default_build == "tmp/build");
    REQUIRE(reloaded.config().m_db_dir == std::filesystem::path("/tmp/custom-db"));

    const fcpp::SLlmModelRole& smart =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::CoderSmart);
    REQUIRE(smart.m_resolved_api == "https://routerai.ru/api/v1");
    REQUIRE(smart.m_resolved_model == "claude-3.5");
    REQUIRE(smart.m_resolved_key == "sk-secret");
    REQUIRE(smart.m_resolved_type == "claude");
    REQUIRE(smart.m_resolved_context_window_tokens == 200000);
    REQUIRE(smart.m_resolved_output_reserve_tokens == 8192);

    const fcpp::SLlmModelRole& local =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::CoderLocal);
    REQUIRE(local.m_resolved_model == "deepseek");
    REQUIRE(local.m_resolved_context_window_tokens == 32768);
    REQUIRE(local.m_resolved_output_reserve_tokens == 2048);

    // Alias takes priority and copies the resolved target parameters.
    const fcpp::SLlmModelRole& thinker_smart =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::ThinkerSmart);
    REQUIRE(thinker_smart.m_resolved_api == smart.m_resolved_api);
    REQUIRE(thinker_smart.m_resolved_model == smart.m_resolved_model);
    REQUIRE(thinker_smart.m_resolved_role == fcpp::ELlmRole::CoderSmart);
    const fcpp::SLlmModelRole& thinker_local =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::ThinkerLocal);
    REQUIRE(thinker_local.m_resolved_model == local.m_resolved_model);
    REQUIRE(thinker_local.m_resolved_role == fcpp::ELlmRole::CoderLocal);

    std::filesystem::remove_all(home);
}

TEST_CASE("config: alias chain resolves through an intermediate role", "[stage8a]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-s8-alias-chain");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    {
        std::ofstream conf(config.config_path());
        conf << "enable_tidy=\"YES\"\n";
        conf << "registry_path=\"" << (home / ".fcpp-agent" / "projects" / "registry.db").string() << "\"\n";
        // CoderSmart holds the real endpoint.
        conf << "llm_coder_smart_api=\"https://routerai.ru/api/v1\"\n";
        conf << "llm_coder_smart_model=\"deepseek/deepseek-v4.1-flash\"\n";
        conf << "llm_coder_smart_key=\"sk-live\"\n";
        conf << "llm_coder_smart_type=\"openai-compat\"\n";
        // Local coder endpoint only, no model name (local server substitutes it).
        conf << "llm_coder_local_api=\"http://127.0.0.1:8080\"\n";
        // ThinkerSmart is an alias of an alias target.
        conf << "llm_thinker_smart_alias=\"CODER_SMART\"\n";
        // ThinkerLocal points at ThinkerSmart, which itself is an alias.
        conf << "llm_thinker_local_alias=\"THINKER_SMART\"\n";
    }

    fcpp::CConfigManager reloaded;
    REQUIRE(reloaded.load());

    const fcpp::SLlmModelRole& smart =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::CoderSmart);
    REQUIRE(smart.m_resolved_model == "deepseek/deepseek-v4.1-flash");
    REQUIRE(smart.m_resolved_role == fcpp::ELlmRole::CoderSmart);

    // Empty model is valid for a local openai-compat endpoint.
    const fcpp::SLlmModelRole& local =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::CoderLocal);
    REQUIRE(local.m_configured);
    REQUIRE(local.m_resolved_api == "http://127.0.0.1:8080");
    REQUIRE(local.m_resolved_model.empty());

    // Chain: THINKER_LOCAL -> THINKER_SMART -> CODER_SMART -> real endpoint.
    const fcpp::SLlmModelRole& thinker_smart =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::ThinkerSmart);
    const fcpp::SLlmModelRole& thinker_local =
        reloaded.config().m_llm_roles.at(fcpp::ELlmRole::ThinkerLocal);
    for (const fcpp::SLlmModelRole* role : {&thinker_smart, &thinker_local})
    {
        REQUIRE(role->m_configured);
        REQUIRE(role->m_resolved_api == "https://routerai.ru/api/v1");
        REQUIRE(role->m_resolved_model == "deepseek/deepseek-v4.1-flash");
        REQUIRE(role->m_resolved_key == "sk-live");
        REQUIRE(role->m_resolved_role == fcpp::ELlmRole::CoderSmart);
    }

    std::filesystem::remove_all(home);
}

TEST_CASE("config: alias cycle is rejected", "[stage8a]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-s8-alias-cycle");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    {
        std::ofstream conf(config.config_path());
        conf << "enable_tidy=\"YES\"\n";
        conf << "registry_path=\"" << (home / ".fcpp-agent" / "projects" / "registry.db").string() << "\"\n";
        conf << "llm_coder_smart_alias=\"THINKER_SMART\"\n";
        conf << "llm_thinker_smart_alias=\"CODER_SMART\"\n";
    }

    fcpp::CConfigManager reloaded;
    REQUIRE_FALSE(reloaded.load());
    std::filesystem::remove_all(home);
}

TEST_CASE("config: invalid model context limits are rejected", "[stage8a]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-s8-invalid-context");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    {
        std::ofstream conf(config.config_path(), std::ios::app);
        conf << "llm_coder_smart_context_window_tokens=\"32k\"\n";
    }

    fcpp::CConfigManager reloaded;
    REQUIRE_FALSE(reloaded.load());
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
