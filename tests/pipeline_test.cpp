#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "core/CRequestPipeline.h"
#include "fcpp-agent/SIntent.h"
#include "fcpp-agent/SRequestOutcome.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_pipe_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_pipe_counter++));
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
 * @brief Сформировать Intent.
 * @param area Область.
 * @param relation Отношение.
 * @param projection Проекция.
 * @param subject Имя субъекта.
 * @return Намерение.
 */
fcpp::SIntent make_intent(fcpp::EArea area,
                          fcpp::ERelation relation,
                          fcpp::EProjection projection,
                          const std::string& subject)
{
    fcpp::SIntent intent;
    intent.m_area = area;
    intent.m_relation = relation;
    intent.m_projection = projection;
    intent.m_subject.m_name = subject;
    intent.m_subject.m_selection = fcpp::ESubjectSelection::ExplicitName;
    return intent;
}

} // namespace

TEST_CASE("pipeline: маршрут существует -> планируется", "[pipeline]")
{
    // Чистый роутинг без проекта (benchmark уровня A).
    fcpp::CRequestPipeline pipeline(nullptr, nullptr);
    const fcpp::SRequestOutcome outcome =
        pipeline.run(make_intent(fcpp::EArea::Understand, fcpp::ERelation::Callers,
                                 fcpp::EProjection::Locations, "Foo::bar"));

    REQUIRE(outcome.m_kind == fcpp::EOutcomeKind::Ok);
    REQUIRE(outcome.m_stage == fcpp::EStage::Planned);
    REQUIRE_FALSE(outcome.m_plan.m_tools.empty());
}

TEST_CASE("pipeline: нет маршрута -> NOT_UNDERSTOOD на VALIDATED", "[pipeline]")
{
    fcpp::CRequestPipeline pipeline(nullptr, nullptr);
    const fcpp::SRequestOutcome outcome =
        pipeline.run(make_intent(fcpp::EArea::Verify, fcpp::ERelation::Callers,
                                 fcpp::EProjection::Graph, "Foo"));

    REQUIRE(outcome.m_kind == fcpp::EOutcomeKind::NotUnderstood);
    REQUIRE(outcome.m_stage == fcpp::EStage::Validated);
}

TEST_CASE("pipeline: пустой субъект -> CLARIFY на RESOLVED", "[pipeline]")
{
    fcpp::CRequestPipeline pipeline(nullptr, nullptr);
    const fcpp::SRequestOutcome outcome =
        pipeline.run(make_intent(fcpp::EArea::Understand, fcpp::ERelation::Self,
                                 fcpp::EProjection::Explanation, ""));

    REQUIRE(outcome.m_kind == fcpp::EOutcomeKind::Clarify);
    REQUIRE(outcome.m_stage == fcpp::EStage::Resolved);
    REQUIRE(outcome.m_options.empty());
    REQUIRE(outcome.m_message.find("ask the user") != std::string::npos);
}

TEST_CASE("pipeline: policy allow_read=NO -> REFUSED", "[pipeline]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-pipe-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    {
        std::ofstream conf(config.config_path(), std::ios::app);
        conf << "allow_read=\"NO\"\n";
    }
    REQUIRE(config.reload_config());

    fcpp::CRequestPipeline pipeline(nullptr, &config);
    const fcpp::SRequestOutcome outcome =
        pipeline.run(make_intent(fcpp::EArea::Understand, fcpp::ERelation::Callers,
                                 fcpp::EProjection::Locations, "Foo::bar"));

    REQUIRE(outcome.m_kind == fcpp::EOutcomeKind::Refused);

    std::filesystem::remove_all(home);
}

TEST_CASE("pipeline: light-restored project is not treated as fully opened", "[pipeline][project]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-pipe-light-home");
    HomeScope scope(home);
    const std::filesystem::path project = make_temp_dir("fcpp-pipe-light-project");
    std::filesystem::create_directories(project / "src");
    std::filesystem::create_directories(project / "build");
    std::ofstream(project / "CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(p LANGUAGES CXX)\n";
    std::ofstream(project / "src" / "a.cpp")
        << "struct A { int load(); };\nint A::load() { return 1; }\n";
    {
        std::ofstream commands(project / "build" / "compile_commands.json");
        const std::string source = (project / "src" / "a.cpp").string();
        commands << "[{\"directory\":\"" << project.string()
                 << "\",\"file\":\"" << source
                 << "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"-c\",\""
                 << source << "\"]}]\n";
    }

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());
    {
        fcpp::CProjectManager opened(config);
        std::string open_failure_stage;
        const bool opened_ok = opened.open(project, &open_failure_stage);
        INFO("project.open failure stage: " << open_failure_stage);
        REQUIRE(opened_ok);
        REQUIRE(opened.is_opened());
        REQUIRE(opened.save_session());
        opened.close();
    }

    fcpp::CProjectManager light(config);
    REQUIRE(light.restore_session_light());
    REQUIRE(light.has_current());
    REQUIRE_FALSE(light.is_opened());

    fcpp::CRequestPipeline pipeline(&light, &config);
    const fcpp::SRequestOutcome outcome =
        pipeline.run(make_intent(fcpp::EArea::Understand, fcpp::ERelation::Self,
                                 fcpp::EProjection::Explanation, "load"));
    REQUIRE(outcome.m_kind == fcpp::EOutcomeKind::Failed);
    REQUIRE(outcome.m_message.find("full project.open") != std::string::npos);

    std::filesystem::remove_all(project);
    std::filesystem::remove_all(home);
}

TEST_CASE("pipeline: интерактивная неоднозначность -> CLARIFY с вариантами", "[pipeline]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-pipe-home2");
    HomeScope scope(home);

    // Минимальный проект с двумя классами, имеющими метод load().
    const std::filesystem::path project = make_temp_dir("fcpp-pipe-proj");
    std::filesystem::create_directories(project / "src");
    std::filesystem::create_directories(project / "build");
    std::ofstream(project / "CMakeLists.txt") << "cmake_minimum_required(VERSION 3.20)\nproject(p LANGUAGES CXX)\n";
    std::ofstream(project / "src" / "a.cpp")
        << "struct A { int load(); int load(int mode); };\n"
           "struct B { int load(); };\n"
           "int A::load() { return 1; }\n"
           "int A::load(int mode) { return mode; }\n"
           "int B::load() { return 2; }\n";

    const std::string directory = project.string();
    const std::string include_flag = "-I" + (project / "src").string();
    {
        std::ofstream commands(project / "build" / "compile_commands.json");
        commands << "[{\"directory\":\"" << directory << "\",\"file\":\"" << (project / "src" / "a.cpp").string()
                 << "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"" << include_flag << "\",\"-c\",\""
                 << (project / "src" / "a.cpp").string() << "\"]}]\n";
    }

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(project));

    fcpp::CRequestPipeline pipeline(&manager, &config);
    const fcpp::SRequestOutcome outcome =
        pipeline.run(make_intent(fcpp::EArea::Understand, fcpp::ERelation::Self,
                                 fcpp::EProjection::Explanation, "load"));

    REQUIRE(outcome.m_kind == fcpp::EOutcomeKind::Clarify);
    REQUIRE(outcome.m_options.size() == 2);

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project);
}
