#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include <sqlite3.h>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/IClang.h"
#include "fcpp-agent/ITool.h"
#include "fcpp-agent/SProject.h"
#include "index/CClangFactory.h"
#include "index/CSQLiteIndex.h"
#include "index/CSqlCompilerIndex.h"
#include "tools/CToolContext.h"
#include "tools/CToolRegistry.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_tools_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_tools_counter++));
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

TEST_CASE("tool registry: имена и разрешение", "[tools]")
{
    const fcpp::CToolRegistry registry;
    REQUIRE_FALSE(registry.names().empty());

    fcpp::EToolName name;
    REQUIRE(fcpp::CToolRegistry::parse_tool_name("callers", name));
    REQUIRE(name == fcpp::EToolName::Callers);
    REQUIRE_FALSE(fcpp::CToolRegistry::parse_tool_name("nope", name));
}

TEST_CASE("tools на dogfood: outline/callers/dispatch/derived/source", "[tools]")
{
    const std::filesystem::path dogfood = dogfood_path();
    if (dogfood.empty())
    {
        SUCCEED("dogfood fixture недоступна — пропуск");
        return;
    }

    const std::filesystem::path home = make_temp_dir("fcpp-tools-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CProjectManager manager(config);
    REQUIRE(manager.open(dogfood));

    const fcpp::SProjectContext& project = manager.current();
    std::unique_ptr<fcpp::IClang> clang = fcpp::create_clang_backend(config.config().m_clang_path);

    fcpp::CToolContext context(project.m_project.m_db_path,
                               project.m_project.m_canonical_path,
                               clang.get());
    REQUIRE(context.open());

    const fcpp::CToolRegistry registry;

    auto run = [&registry, &context](fcpp::EToolName tool, const std::string& target)
    {
        fcpp::SToolRequest request;
        request.m_tool = tool;
        request.m_target = target;
        return registry.execute(tool, request, context);
    };

    const fcpp::SToolResult outline = run(fcpp::EToolName::Outline, "Shape");
    REQUIRE(outline.m_ok);
    REQUIRE(outline.m_symbols.size() >= 2);

    const fcpp::SToolResult callers = run(fcpp::EToolName::Callers, "Shape::area");
    REQUIRE(callers.m_ok);
    REQUIRE_FALSE(callers.m_symbols.empty());

    const fcpp::SToolResult dispatch = run(fcpp::EToolName::Dispatch, "Shape::area");
    REQUIRE(dispatch.m_ok);
    REQUIRE(dispatch.m_names.size() == 3);

    const fcpp::SToolResult derived = run(fcpp::EToolName::Derived, "Shape");
    REQUIRE(derived.m_ok);
    REQUIRE(derived.m_names.size() == 2);

    const fcpp::SToolResult source = run(fcpp::EToolName::Source, "Shape::area");
    REQUIRE(source.m_ok);
    REQUIRE_FALSE(source.m_text.empty());

    const fcpp::SToolResult missing = run(fcpp::EToolName::Callers, "NoSuchSymbol");
    REQUIRE_FALSE(missing.m_ok);
    REQUIRE(missing.m_error == fcpp::EErrorKind::SymbolNotFound);

    std::filesystem::remove_all(home);
}
