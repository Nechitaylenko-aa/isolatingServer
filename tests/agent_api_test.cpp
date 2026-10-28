#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "core/CAgentApi.h"
#include "core/CConfigManager.h"
#include "fcpp-agent/IAgentApi.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_api_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_api_counter++));
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
 * @return Путь.
 */
std::filesystem::path make_project()
{
    const std::filesystem::path project = make_temp_dir("fcpp-api-proj");
    std::filesystem::create_directories(project / "src");
    std::filesystem::create_directories(project / "build");
    std::ofstream(project / "CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(api LANGUAGES CXX)\n";
    std::ofstream(project / "src" / "main.cpp") << "int main() { return 0; }\n";
    std::ofstream commands(project / "build" / "compile_commands.json");
    commands << "[{\"directory\":\"" << project.string() << "\",\"file\":\""
             << (project / "src" / "main.cpp").string() << "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"-c\",\""
             << (project / "src" / "main.cpp").string() << "\"]}]\n";
    return project;
}

} // namespace

TEST_CASE("agent API: open/status/tool/patch через единый интерфейс", "[api]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-api-home");
    HomeScope scope(home);

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_project();

    fcpp::CAgentApi api(config);

    fcpp::SApiRequest status_request;
    status_request.m_method = fcpp::EApiMethod::AgentStatus;
    REQUIRE(api.execute(status_request).m_ok);

    // Failed project opens expose a stable diagnostic stage to the IDE/log.
    fcpp::SApiRequest invalid_open;
    invalid_open.m_method = fcpp::EApiMethod::ProjectOpen;
    invalid_open.m_path = (home / "missing-project-root").string();
    const fcpp::SApiResponse invalid_open_response = api.execute(invalid_open);
    REQUIRE_FALSE(invalid_open_response.m_ok);
    REQUIRE(invalid_open_response.m_message.find("canonicalize_project_root") != std::string::npos);

    // Conversation sessions have a lifecycle independent of project sessions.
    fcpp::SApiRequest session_open;
    session_open.m_method = fcpp::EApiMethod::SessionOpen;
    const fcpp::SApiResponse session_response = api.execute(session_open);
    REQUIRE(session_response.m_ok);
    REQUIRE_FALSE(session_response.m_session_id.empty());

    // resume требует открытого проекта: иначе адрес диалога не определён.
    fcpp::SApiRequest resume_without_project;
    resume_without_project.m_method = fcpp::EApiMethod::SessionResume;
    const fcpp::SApiResponse resume_rejected = api.execute(resume_without_project);
    REQUIRE_FALSE(resume_rejected.m_ok);
    REQUIRE(resume_rejected.m_error == fcpp::EErrorKind::ProjectNotOpen);

    // Supplying a projectId must not bypass the open-project requirement.
    fcpp::SApiRequest resume_arbitrary_project;
    resume_arbitrary_project.m_method = fcpp::EApiMethod::SessionResume;
    resume_arbitrary_project.m_project_id = "project-that-is-not-open";
    const fcpp::SApiResponse arbitrary_resume = api.execute(resume_arbitrary_project);
    REQUIRE_FALSE(arbitrary_resume.m_ok);
    REQUIRE(arbitrary_resume.m_error == fcpp::EErrorKind::ProjectNotOpen);

    fcpp::SApiRequest update_context;
    update_context.m_method = fcpp::EApiMethod::SessionUpdateContext;
    update_context.m_session_id = session_response.m_session_id;
    update_context.m_context_json = R"({"goal":"inspect API","open_questions":["history injection"]})";
    REQUIRE(api.execute(update_context).m_ok);

    fcpp::SApiRequest close_conversation;
    close_conversation.m_method = fcpp::EApiMethod::SessionClose;
    close_conversation.m_session_id = session_response.m_session_id;
    REQUIRE(api.execute(close_conversation).m_ok);
    REQUIRE_FALSE(api.execute(close_conversation).m_ok);

    fcpp::SApiRequest open_request;
    open_request.m_method = fcpp::EApiMethod::ProjectOpen;
    open_request.m_path = project_dir.string();
    const fcpp::SApiResponse open_response = api.execute(open_request);
    REQUIRE(open_response.m_ok);
    REQUIRE_FALSE(open_response.m_project_id.empty());
    // «Проект открыт» = «проект адресуем»: клиенту не нужно угадывать
    // необходимость session.open для нового проекта.
    REQUIRE_FALSE(open_response.m_session_id.empty());

    fcpp::SApiRequest open_again;
    open_again.m_method = fcpp::EApiMethod::ProjectOpen;
    open_again.m_path = project_dir.string();
    const fcpp::SApiResponse reopened = api.execute(open_again);
    REQUIRE(reopened.m_ok);
    REQUIRE(reopened.m_session_id == open_response.m_session_id);

    // После перезапуска клиент восстанавливает адрес диалога у агента,
    // хранящего id в БД, а не у себя.
    fcpp::SApiRequest session_resume;
    session_resume.m_method = fcpp::EApiMethod::SessionResume;
    const fcpp::SApiResponse resumed = api.execute(session_resume);
    REQUIRE(resumed.m_ok);
    REQUIRE(resumed.m_session_id == open_response.m_session_id);
    REQUIRE(resumed.m_project_id == open_response.m_project_id);

    fcpp::SApiRequest profile_request;
    profile_request.m_method = fcpp::EApiMethod::ProjectProfileSet;
    profile_request.m_profile_text = "C++20; do not change the public API";
    const fcpp::SApiResponse profile_response = api.execute(profile_request);
    REQUIRE(profile_response.m_ok);
    REQUIRE(profile_response.m_project_id == open_response.m_project_id);

    fcpp::SApiRequest close_before_profile;
    close_before_profile.m_method = fcpp::EApiMethod::ProjectClose;
    REQUIRE(api.execute(close_before_profile).m_ok);
    REQUIRE_FALSE(api.execute(profile_request).m_ok);
    REQUIRE(api.execute(open_request).m_ok);
    REQUIRE(api.execute(profile_request).m_ok);

    fcpp::SApiRequest index_request;
    index_request.m_method = fcpp::EApiMethod::IndexStatus;
    const fcpp::SApiResponse index_response = api.execute(index_request);
    REQUIRE(index_response.m_ok);
    REQUIRE(index_response.m_index_status.m_symbol_count > 0);

    // Tool: callers по несуществующему символу -> типизированная ошибка.
    fcpp::SApiRequest tool_request;
    tool_request.m_method = fcpp::EApiMethod::Tool;
    tool_request.m_tool = "callers";
    tool_request.m_target = "NoSuch";
    const fcpp::SApiResponse tool_response = api.execute(tool_request);
    REQUIRE_FALSE(tool_response.m_ok);
    REQUIRE(tool_response.m_error == fcpp::EErrorKind::SymbolNotFound);

    // Patch через API.
    const std::string diff =
        "--- a/src/main.cpp\n"
        "+++ b/src/main.cpp\n"
        "@@ -1,1 +1,1 @@\n"
        "-int main() { return 0; }\n"
        "+int main() { return 1; }\n";

    fcpp::SApiRequest patch_request;
    patch_request.m_method = fcpp::EApiMethod::Patch;
    patch_request.m_payload = diff;
    patch_request.m_assume_yes = true;
    const fcpp::SApiResponse patch_response = api.execute(patch_request);
    REQUIRE(patch_response.m_ok);

    std::ifstream changed(project_dir / "src" / "main.cpp");
    const std::string content((std::istreambuf_iterator<char>(changed)), std::istreambuf_iterator<char>());
    REQUIRE(content.find("return 1") != std::string::npos);

    fcpp::SApiRequest close_request;
    close_request.m_method = fcpp::EApiMethod::ProjectClose;
    REQUIRE(api.execute(close_request).m_ok);

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}


TEST_CASE("agent API: lifecycle diagnostics are JSONL and correlate request IDs", "[api][logging]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-api-log-home");
    HomeScope scope(home);
    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CAgentApi api(config);
    fcpp::SApiRequest request;
    request.m_request_id = "test-request-42";
    request.m_method = fcpp::EApiMethod::AgentStatus;
    REQUIRE(api.execute(request).m_ok);

    const std::filesystem::path log_path = config.agent_dir() / "logs" / "agent.jsonl";
    REQUIRE(std::filesystem::exists(log_path));
    std::ifstream input(log_path);
    REQUIRE(input.is_open());
    std::string line;
    std::vector<nlohmann::json> records;
    while (std::getline(input, line))
    {
        REQUIRE_NOTHROW(nlohmann::json::parse(line));
        records.push_back(nlohmann::json::parse(line));
    }
    REQUIRE(records.size() == 2);
    REQUIRE(records[0]["event"] == "request.start");
    REQUIRE(records[0]["request_id"] == "test-request-42");
    REQUIRE(records[0]["method"] == "agent.status");
    REQUIRE(records[1]["event"] == "request.finish");
    REQUIRE(records[1]["request_id"] == "test-request-42");
    REQUIRE(records[1]["ok"] == true);
    REQUIRE(records[1]["duration_ms"].is_number_integer());
    REQUIRE_FALSE(records[0].contains("text"));
    std::filesystem::remove_all(home);
}


TEST_CASE("agent API: ask is routed through the new orchestration tree", "[api][ask]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-api-ask-tree-home");
    HomeScope scope(home);
    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    fcpp::CAgentApi api(config);
    fcpp::SApiRequest session_open;
    session_open.m_method = fcpp::EApiMethod::SessionOpen;
    const fcpp::SApiResponse session = api.execute(session_open);
    REQUIRE(session.m_ok);

    fcpp::SApiRequest request;
    request.m_method = fcpp::EApiMethod::Ask;
    request.m_session_id = session.m_session_id;
    request.m_text = "Что делает этот метод?";
    const fcpp::SApiResponse response = api.execute(request);

    REQUIRE(response.m_ok);
    REQUIRE(response.m_state == "COMPLETED");
    REQUIRE(response.m_session_id == session.m_session_id);
    REQUIRE_FALSE(response.m_has_question);
    REQUIRE(response.m_text.find("ORCHESTRATION_STATUS=STUB_COMPLETE") != std::string::npos);
    REQUIRE(response.m_text.find("EXIT_REASON=SCOPE_CLASSIFICATION_NOT_IMPLEMENTED") != std::string::npos);
    REQUIRE(response.m_text.find("ANSWER_GENERATION:SKIPPED_UPSTREAM_STUB") != std::string::npos);

    std::filesystem::remove_all(home);
}

TEST_CASE("agent API: cursor metadata reaches orchestration tree without legacy routing", "[api][ask][cursor]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-api-cursor-tree-home");
    HomeScope scope(home);
    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    REQUIRE(config.load());

    const std::filesystem::path project_dir = make_project();
    fcpp::CAgentApi api(config);

    fcpp::SApiRequest open_request;
    open_request.m_method = fcpp::EApiMethod::ProjectOpen;
    open_request.m_path = project_dir.string();
    const fcpp::SApiResponse opened = api.execute(open_request);
    REQUIRE(opened.m_ok);
    REQUIRE_FALSE(opened.m_session_id.empty());

    fcpp::SApiRequest request;
    request.m_method = fcpp::EApiMethod::Ask;
    request.m_session_id = opened.m_session_id;
    request.m_text = "А что он делает?";
    request.m_editor_file = (project_dir / "src" / "missing.cpp").string();
    request.m_editor_line = 5;
    request.m_editor_column = 1;

    const fcpp::SApiResponse response = api.execute(request);
    REQUIRE(response.m_ok);
    REQUIRE(response.m_state == "COMPLETED");
    REQUIRE(response.m_text.find("CURSOR_APPLICABILITY:STUB_NOT_APPLICABLE") != std::string::npos);
    REQUIRE(response.m_text.find("EXIT_REASON=SCOPE_CLASSIFICATION_NOT_IMPLEMENTED") != std::string::npos);

    std::filesystem::remove_all(home);
    std::filesystem::remove_all(project_dir);
}
