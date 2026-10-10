#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

#include "core/CConfigManager.h"
#include "core/CEvidenceCollector.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SEvidence.h"
#include "fcpp-agent/SIntent.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_ev_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_ev_counter++));
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
 * @brief Путь к обкаточному проекту.
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

TEST_CASE("evidence formatter собирает текст секций", "[evidence]")
{
    fcpp::SEvidence evidence;
    fcpp::SEvidenceSection section;
    section.m_tool = fcpp::EToolName::Callers;
    section.m_title = "callers";
    section.m_text = "function total_area\n";
    evidence.m_sections.push_back(section);

    const std::string text = fcpp::format_evidence(evidence);
    REQUIRE(text.find("[callers]") != std::string::npos);
    REQUIRE(text.find("total_area") != std::string::npos);
}

TEST_CASE("evidence formatter пропускает пустые секции", "[evidence]")
{
    fcpp::SEvidence evidence;
    fcpp::SEvidenceSection empty;
    empty.m_title = "empty";
    evidence.m_sections.push_back(empty);

    REQUIRE(fcpp::format_evidence(evidence).empty());
}

TEST_CASE("evidence collector собирает факты по плану на dogfood", "[evidence]")
{
    const std::filesystem::path dogfood = dogfood_path();
    if (dogfood.empty())
    {
        SUCCEED("dogfood fixture недоступна — пропуск");
        return;
    }

    const std::filesystem::path home = make_temp_dir("fcpp-ev-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(dogfood));

    fcpp::SIntent intent;
    intent.m_area = fcpp::EArea::Understand;
    intent.m_relation = fcpp::ERelation::Callers;
    intent.m_projection = fcpp::EProjection::Locations;
    intent.m_subject.m_name = "Shape::area";
    intent.m_subject.m_kind = fcpp::ESubjectKind::Method;

    fcpp::SPlan plan;
    plan.m_tools = {fcpp::EToolName::Callers};

    fcpp::CEvidenceCollector collector(config, manager);
    fcpp::SEvidence evidence;
    REQUIRE(collector.collect(intent, plan, evidence));
    REQUIRE_FALSE(evidence.m_sections.empty());

    const std::string text = fcpp::format_evidence(evidence);
    REQUIRE(text.find("total_area") != std::string::npos);

    std::filesystem::remove_all(home);
}
