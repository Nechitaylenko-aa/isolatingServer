/**
 * @file orchestrator_routes_test.cpp
 * @brief Маршруты дерева оркестратора и его отказные пути.
 *
 * Проверяются две вещи, которые нельзя увидеть по отдельным компонентам:
 * 1. Каждый маршрут собирает ровно тот контекст, который ему назначен
 *    (Zero-Window: подтверждённые факты вытесняют сырой файл).
 * 2. Любой сбой завершает запрос видимым исходом, а не молчаливым ответом:
 *    причина выхода, завершающее событие следа и запись `request.finished`.
 */

#include "core/CConfigManager.h"
#include "core/CCursorContextAnalyzer.h"
#include "core/CHistorian.h"
#include "core/CLogger.h"
#include "core/CProjectManager.h"
#include "core/CRequestOrchestrator.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "fcpp-agent/IClang.h"
#include "fcpp-agent/IFactProvider.h"
#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/SLlmRoleConfig.h"

namespace
{

/**
 * @brief Сценарий ответов модели, общий для всех ролей.
 *
 * Роль не влияет на сценарий: тесты проверяют, какую роль вызвал оркестратор,
 * а не то, что модель знает о своей роли.
 */
struct SScript
{
    std::string m_relevance = "YES"; ///< Ответ на YES/NO о курсоре или файле.
    std::string m_selection = "NONE"; ///< Ответ историку (номера пар или NONE).
    std::string m_entity = "SEVERAL"; ///< Ответ на выбор сущности файла.
    std::string m_answer = "answer";  ///< Итоговый ответ модели.
    std::string m_intent = "NO";      ///< Ответ классификатору intent (YES = Edit).
    std::string m_patch;              ///< Диф, который модель вернёт для CppPatch.

    /// Все обращения: роль и пользовательский текст сообщения.
    std::vector<std::pair<fcpp::ELlmRole, std::string>> m_calls;

    /**
     * @brief Последний текст, отправленный указанной роли.
     * @param role Роль.
     * @return Текст либо пустая строка.
     */
    std::string last_text(fcpp::ELlmRole role) const
    {
        for (auto it = m_calls.rbegin(); it != m_calls.rend(); ++it)
        {
            if (it->first == role)
            {
                return it->second;
            }
        }
        return {};
    }

    /**
     * @brief Сколько раз вызывалась роль.
     * @param role Роль.
     * @return Количество обращений.
     */
    std::size_t count_of(fcpp::ELlmRole role) const
    {
        std::size_t total = 0;
        for (const auto& call : m_calls)
        {
            if (call.first == role)
            {
                ++total;
            }
        }
        return total;
    }
};

/**
 * @brief Модель, отвечающая по контракту промпта.
 */
class CScriptedModel final : public fcpp::IModelApi
{
public:
    CScriptedModel(fcpp::ELlmRole role, SScript& script)
        : m_role(role), m_script(script)
    {
    }

    bool is_available() const override
    {
        return true;
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>& messages) override
    {
        const std::string system = messages.empty() ? std::string{} : messages.front().m_content;
        const std::string input =
            messages.size() > 1 ? messages.back().m_content : std::string{};
        m_script.m_calls.emplace_back(m_role, input);

        if (system.find("Select the prior conversation turns") != std::string::npos)
        {
            return reply(m_script.m_selection);
        }
        if (system.find("Identify whether answering requires one") != std::string::npos)
        {
            return reply(m_script.m_entity);
        }
        if (system.find("Decide whether the C++ entity") != std::string::npos
            || system.find("concerns the supplied file") != std::string::npos)
        {
            return reply(m_script.m_relevance);
        }
        // Намеренно НЕ "YesNo" целиком: система промпта YesNo — общий
        // контракт, единственный текущий вызывающий — classify_intent().
        // Различаем по тому же принципу, что и остальные ветки — по тексту
        // system-промпта, не по EPromptType (фейк не видит enum).
        if (system.find("Determine whether the statement or condition") != std::string::npos)
        {
            return reply(m_script.m_intent);
        }
        if (system.find("Produce a single unified diff") != std::string::npos)
        {
            return reply(m_script.m_patch);
        }
        return reply(m_script.m_answer);
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>& messages,
                               const std::vector<fcpp::SModelTool>&) override
    {
        return complete(messages);
    }

    fcpp::SModelReply complete_stream(const std::vector<fcpp::SModelMessage>& messages,
                                      const fcpp::SStreamCallback&) override
    {
        return complete(messages);
    }

private:
    fcpp::SModelReply reply(const std::string& text) const
    {
        return fcpp::SModelReply{true, text, {}, "stop", {}};
    }

    fcpp::ELlmRole m_role;
    SScript& m_script;
};

/**
 * @brief Бэкенд clang с заранее заданной сущностью под курсором.
 */
class CFixedSymbolClang final : public fcpp::IClang
{
public:
    explicit CFixedSymbolClang(std::optional<fcpp::SClangSymbol> symbol)
        : m_symbol(std::move(symbol))
    {
    }

    bool is_available() const override
    {
        return true;
    }

    std::string version() const override
    {
        return "test";
    }

    bool extract(const std::filesystem::path&, const std::vector<std::string>&,
                 fcpp::SClangExtract&) const override
    {
        return true;
    }

    std::optional<fcpp::SClangSymbol> cursor_at(
        const std::filesystem::path&, std::uint32_t, std::uint32_t,
        const std::vector<std::string>&,
        fcpp::EFocusKind = fcpp::EFocusKind::Innermost) const override
    {
        return m_symbol;
    }

private:
    std::optional<fcpp::SClangSymbol> m_symbol;
};

/**
 * @brief Поставщик фактов с управляемым результатом.
 */
class CFakeFactProvider final : public fcpp::IFactProvider
{
public:
    bool m_ok = true;                        ///< Технический успех сбора.
    bool m_rejected = false;                 ///< Сущность не из ожидаемого файла.
    std::string m_error = "index db is not readable"; ///< Причина сбоя.
    std::vector<fcpp::SFactSection> m_sections; ///< Секции при успехе.
    mutable std::size_t m_calls = 0;         ///< Сколько раз спрашивали.

    fcpp::SFactResult collect(const fcpp::SFactRequest&) const override
    {
        ++m_calls;
        fcpp::SFactResult result;
        result.m_ok = m_ok;
        result.m_rejected = m_rejected;
        if (!m_ok)
        {
            result.m_error = m_error;
            return result;
        }
        if (!m_rejected)
        {
            result.m_sections = m_sections;
        }
        return result;
    }
};

/** @brief Создать пустой временный каталог. */
std::filesystem::path make_temp_dir(const std::string& name)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::create_directories(dir, ignored);
    return dir;
}

/** @brief Записать текстовый файл. */
void write_file(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path);
    REQUIRE(output.is_open());
    output << content;
}

/** @brief Прочитать журнал целиком. */
std::string read_log(const fcpp::CLogger& logger)
{
    std::ifstream input(logger.path());
    if (!input.is_open())
    {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

/** @brief Сколько раз событие встречается в журнале. */
std::size_t count_events(const std::string& log_text, const std::string& event)
{
    const std::string needle = "\"event\":\"" + event + "\"";
    std::size_t total = 0;
    std::size_t position = log_text.find(needle);
    while (position != std::string::npos)
    {
        ++total;
        position = log_text.find(needle, position + needle.size());
    }
    return total;
}

/**
 * @brief Собрать clang-бэкенд с сущностью внутри файла.
 * @param file Файл.
 * @param name Короткое и квалифицированное имя.
 * @return Бэкенд.
 */
std::unique_ptr<CFixedSymbolClang> make_clang(
    const std::filesystem::path& file, const std::string& name)
{
    fcpp::SClangSymbol symbol{};
    symbol.m_name = name;
    symbol.m_qualified_name = name;
    symbol.m_signature = "class " + name;
    symbol.m_usr = "c:@S@" + name;
    symbol.m_kind = fcpp::ESymbolKind::Class;
    symbol.m_location.m_file = file.string();
    symbol.m_location.m_line = 1;
    symbol.m_location.m_column = 1;
    symbol.m_location.m_end_line = 5;
    symbol.m_is_definition = true;
    return std::make_unique<CFixedSymbolClang>(std::move(symbol));
}

/**
 * @brief Роли с разными окнами у удалённой и локальной модели.
 *
 * Окна заданы явно, поэтому проба провайдера не выполняется и тест не ходит в
 * сеть: именно это и нужно, чтобы видеть, по какой роли посчитан бюджет.
 *
 * @param smart_window Окно удалённой роли (отвечает на курсорных маршрутах).
 * @param local_window Окно локальной роли (отвечает на историю и общий вопрос).
 * @return Набор ролей.
 */
fcpp::SLlmRoleSet make_roles(std::size_t smart_window, std::size_t local_window)
{
    fcpp::SLlmRoleSet roles;
    const auto configure = [](fcpp::SLlmModelRole& role, fcpp::ELlmRole which,
                              std::size_t window)
    {
        role.m_api = "http://127.0.0.1:9/v1";
        role.m_model = "test";
        role.m_type = "openai-compat";
        role.m_configured = true;
        role.m_resolved_api = role.m_api;
        role.m_resolved_model = role.m_model;
        role.m_resolved_type = role.m_type;
        role.m_resolved_role = which;
        role.m_resolved_context_window_tokens = window;
        role.m_resolved_output_reserve_tokens = 1024;
    };
    configure(roles.at(fcpp::ELlmRole::CoderSmart), fcpp::ELlmRole::CoderSmart, smart_window);
    configure(roles.at(fcpp::ELlmRole::CoderLocal), fcpp::ELlmRole::CoderLocal, local_window);
    configure(roles.at(fcpp::ELlmRole::ThinkerSmart), fcpp::ELlmRole::ThinkerSmart, smart_window);
    configure(roles.at(fcpp::ELlmRole::ThinkerLocal), fcpp::ELlmRole::ThinkerLocal, local_window);
    return roles;
}

/**
 * @brief RAII-переопределение HOME — та же схема, что в agent_api_test.cpp
 * и stage8a_test.cpp, нужна CConfigManager для поиска ~/.fcpp-agent.
 */
class HomeScope
{
public:
    explicit HomeScope(const std::filesystem::path& home)
    {
        const char* previous = std::getenv("HOME");
        if (previous != nullptr)
        {
            m_previous = previous;
        }
        setenv("HOME", home.string().c_str(), 1);
    }

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
 * @brief Минимальная структура проекта, которую CProjectManager::open()
 * ожидает увидеть (как в agent_api_test.cpp/stage8a_test.cpp) — CMakeLists.txt
 * и build/compile_commands.json, помимо собственно правимого файла.
 * @param dir Каталог проекта (уже существует).
 * @param source Путь к файлу, который будет правиться (внутри dir).
 */
void scaffold_project(const std::filesystem::path& dir, const std::filesystem::path& source)
{
    std::filesystem::create_directories(dir / "build");
    std::ofstream(dir / "CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(edit_fixture LANGUAGES CXX)\n";
    std::ofstream commands(dir / "build" / "compile_commands.json");
    commands << "[{\"directory\":\"" << dir.string() << "\",\"file\":\""
             << source.string() << "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"-c\",\""
             << source.string() << "\"]}]\n";
}

/**
 * @brief Настроить CConfigManager с заданными allow_write/allow_exec, в
 * изолированном HOME.
 * @param home Каталог HOME (из make_temp_dir).
 * @param allow_write Разрешить запись.
 * @param allow_exec Разрешить исполнение (сборку).
 * @return Сконфигурированный менеджер.
 */
    fcpp::CConfigManager make_config(const std::filesystem::path& /*home*/,
                                     bool allow_write, bool allow_exec)
{
    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    {
        std::ofstream conf(config.config_path());
        conf << "allow_read=\"YES\"\n";
        conf << "allow_write=\"" << (allow_write ? "YES" : "NO") << "\"\n";
        conf << "allow_exec=\"" << (allow_exec ? "YES" : "NO") << "\"\n";
    }
    REQUIRE(config.load());
    return config;
}

} // namespace

TEST_CASE("route plan resolves to the role that actually answers",
          "[orchestrator][budget][route-role]")
{
    // Invariant of the whole budget: every route must name the role that the
    // answer step really calls. If this drifts, budget is planned against one
    // window and the answer overflows another.
    REQUIRE(fcpp::answer_role_for_route(
                fcpp::EOrchestrationRoute::CursorContextRequired) == fcpp::ELlmRole::CoderSmart);
    REQUIRE(fcpp::answer_role_for_route(
                fcpp::EOrchestrationRoute::CursorAndHistoryContextRequired) == fcpp::ELlmRole::CoderSmart);
    REQUIRE(fcpp::answer_role_for_route(
                fcpp::EOrchestrationRoute::FileContextRequired) == fcpp::ELlmRole::CoderSmart);
    REQUIRE(fcpp::answer_role_for_route(
                fcpp::EOrchestrationRoute::FileAndHistoryContextRequired) == fcpp::ELlmRole::CoderSmart);
    REQUIRE(fcpp::answer_role_for_route(
                fcpp::EOrchestrationRoute::HistoryContextRequired) == fcpp::ELlmRole::CoderLocal);
    REQUIRE(fcpp::answer_role_for_route(
                fcpp::EOrchestrationRoute::NoApplicableContext) == fcpp::ELlmRole::CoderLocal);
    REQUIRE(fcpp::answer_role_for_route(
                fcpp::EOrchestrationRoute::Unselected) == fcpp::ELlmRole::Count);
}

TEST_CASE("history route is budgeted against the local window, not the remote one",
          "[orchestrator][budget][route-role]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-budget-local");

    SScript script;
    script.m_selection = "1";
    script.m_answer = "history answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    // Remote window 57344, local window 8192: the difference is unmistakable.
    const fcpp::SLlmRoleSet roles = make_roles(57344, 8192);
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });
    fcpp::CHistorian historian(llm);
    fcpp::CRequestOrchestrator orchestrator(logger, &historian, nullptr, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-budget-local";
    request.m_text = "What did I ask you to remember?";
    request.m_history = {{1, "Remember the red bicycle", "I will keep that."}};

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::HistoryContextRequired);
    // The budget recorded in the result must be the local one: the answer went
    // to CoderLocal, so sizing it against CoderSmart would be a lie.
    REQUIRE(result.m_budget.m_ok);
    REQUIRE(result.m_budget.m_window == 8192);

    const std::string log_text = read_log(logger);
    REQUIRE(log_text.find("history answer") == std::string::npos);
    // Two fit checks on entry (both candidate roles), then the real role budget.
    REQUIRE(count_events(log_text, "context.request_fit") == 2);
    REQUIRE(count_events(log_text, "context.budget") == 1);
    REQUIRE(log_text.find(R"("role":"CODER_LOCAL")") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("cursor route is budgeted against the remote window",
          "[orchestrator][budget][route-role]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-budget-remote");
    const std::filesystem::path source = dir / "widget.h";
    write_file(source, "class CWidget\n{\npublic:\n    int value() const;\n};\n");

    SScript script;
    script.m_relevance = "YES";
    script.m_answer = "cursor answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    const fcpp::SLlmRoleSet roles = make_roles(57344, 8192);
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });
    const auto clang = make_clang(source, "CWidget");
    fcpp::CCursorContextAnalyzer analyzer(*clang, llm);
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-budget-remote";
    request.m_text = "What does this class do?";
    request.m_cursor_file = source.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::CursorContextRequired);
    REQUIRE(result.m_budget.m_ok);
    // The cursor route answers with the smart coder, so its window applies.
    REQUIRE(result.m_budget.m_window == 57344);
    REQUIRE(result.m_budget.m_cursor > 30000);

    const std::string log_text = read_log(logger);
    REQUIRE(log_text.find(R"("role":"CODER_SMART")") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("request that fits the remote window but not the local one is refused",
          "[orchestrator][budget][route-role]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-budget-refuse");

    SScript script;
    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    const fcpp::SLlmRoleSet roles = make_roles(57344, 2048);
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });
    fcpp::CHistorian historian(llm);
    fcpp::CRequestOrchestrator orchestrator(logger, &historian, nullptr, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-budget-refuse";
    // ~3300 tokens: fits 57344-1024, does not fit 2048-1024.
    request.m_text = std::string(10000, 'x');
    request.m_history = {{1, "Anything", "Something."}};

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    // Refused on entry: the route could have been answered by the local model,
    // so admitting the request would risk an overflow nobody planned for.
    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::RequestTooLong);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::Unselected);
    REQUIRE(script.m_calls.empty());

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "context.request_fit") == 2);
    REQUIRE(count_events(log_text, "request.finished") == 1);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("cursor route sends the slice and index facts, never the history",
          "[orchestrator][route][cursor]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-cursor");
    const std::filesystem::path source = dir / "widget.h";
    write_file(source, "class CWidget\n{\npublic:\n    int value() const;\n};\n");

    SScript script;
    script.m_relevance = "YES";
    script.m_answer = "cursor answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const auto clang = make_clang(source, "CWidget");
    fcpp::CCursorContextAnalyzer analyzer(*clang, llm);

    CFakeFactProvider facts;
    facts.m_sections = {{"Definition", "class CWidget { int value() const; };"}};

    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm, &facts);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-cursor";
    request.m_session_id = "route-session";
    request.m_text = "What does this class do?";
    request.m_cursor_file = source.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_status == fcpp::ERequestStatus::Completed);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::CursorContextRequired);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::None);
    REQUIRE(result.m_message == "cursor answer");
    REQUIRE(result.m_budget.m_ok);
    REQUIRE(result.m_budget.m_cursor > 0);

    // Facts are requested exactly once, for the entity under the cursor.
    REQUIRE(facts.m_calls == 1);
    REQUIRE(result.m_fact_sections.size() == 1);

    // Relevance and intent are both decided by the local thinker (two
    // separate ThinkerLocal calls), the answer by the smart coder.
    REQUIRE(script.count_of(fcpp::ELlmRole::ThinkerLocal) == 2);
    REQUIRE(script.count_of(fcpp::ELlmRole::CoderSmart) == 1);

    const std::string answer_prompt = script.last_text(fcpp::ELlmRole::CoderSmart);
    REQUIRE(answer_prompt.find("CURSOR SOURCE CONTEXT (extracted by Clang)") != std::string::npos);
    REQUIRE(answer_prompt.find("2: {") != std::string::npos);
    REQUIRE(answer_prompt.find("PROJECT INDEX FACTS [Definition]") != std::string::npos);
    REQUIRE(answer_prompt.find("What does this class do?") != std::string::npos);
    // No history was supplied, so no history block may appear.
    REQUIRE(answer_prompt.find("RELEVANT CONVERSATION HISTORY") == std::string::npos);

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "request.started") == 1);
    REQUIRE(count_events(log_text, "request.finished") == 1);
    REQUIRE(count_events(log_text, "request.route") == 1);
    REQUIRE(log_text.find("CURSOR_CONTEXT_REQUIRED") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("index failure on the cursor route ends the request visibly",
          "[orchestrator][route][failure][index]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-index-failure");
    const std::filesystem::path source = dir / "widget.h";
    write_file(source, "class CWidget\n{\npublic:\n    int value() const;\n};\n");

    SScript script;
    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const auto clang = make_clang(source, "CWidget");
    fcpp::CCursorContextAnalyzer analyzer(*clang, llm);

    CFakeFactProvider facts;
    facts.m_ok = false;
    facts.m_error = "index db is not readable: stage=open_db";

    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm, &facts);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-index-failure";
    request.m_text = "Explain this class";
    request.m_cursor_file = source.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_status == fcpp::ERequestStatus::Failed);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::IndexUnavailable);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::CursorContextRequired);
    REQUIRE(result.m_trace.back() == fcpp::ETraceEvent::ExitIndexUnavailable);
    REQUIRE(result.m_message.find("index db is not readable") != std::string::npos);

    // No answer is attempted when the index is unavailable.
    REQUIRE(script.count_of(fcpp::ELlmRole::CoderSmart) == 0);

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "request.finished") == 1);
    REQUIRE(log_text.find("INDEX_UNAVAILABLE") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("unreadable cursor source fails the applicability assessment",
          "[orchestrator][route][failure][cursor]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-cursor-failure");
    const std::filesystem::path missing = dir / "gone.h";

    SScript script;
    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    // The entity resolves, but the source cannot be read: this is a failure,
    // not a missing context.
    const auto clang = make_clang(missing, "CWidget");
    fcpp::CCursorContextAnalyzer analyzer(*clang, llm);
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-cursor-failure";
    request.m_text = "Explain this class";
    request.m_cursor_file = missing.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::CursorRelevanceFailed);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::Unselected);
    REQUIRE(result.m_trace.back() == fcpp::ETraceEvent::ExitCursorRelevanceFailed);
    REQUIRE(result.m_message == "Context applicability assessment failed.");

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "request.finished") == 1);
    REQUIRE(log_text.find("CURSOR_RELEVANCE_FAILED") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("history contract violation ends the request with HistoryRelevanceFailed",
          "[orchestrator][route][failure][history]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-history-failure");

    SScript script;
    // Both the local and the smart thinker violate the numbered-list contract.
    script.m_selection = "the second pair probably";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });
    fcpp::CHistorian historian(llm);
    fcpp::CRequestOrchestrator orchestrator(logger, &historian, nullptr, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-history-failure";
    request.m_text = "What did I ask you to remember?";
    request.m_history = {{1, "Remember the red bicycle", "I will keep that."}};

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::HistoryRelevanceFailed);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::Unselected);
    REQUIRE(result.m_trace.back() == fcpp::ETraceEvent::ExitHistoryRelevanceFailed);
    REQUIRE(script.count_of(fcpp::ELlmRole::ThinkerLocal) == 1);
    REQUIRE(script.count_of(fcpp::ELlmRole::ThinkerSmart) == 1);

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "request.finished") == 1);
    REQUIRE(log_text.find("HISTORY_RELEVANCE_FAILED") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("request longer than the model budget is rejected before context gathering",
          "[orchestrator][route][failure][budget]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-too-long");

    SScript script;
    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, nullptr, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-too-long";
    // Far above the fallback window of 8192 tokens at roughly 3 bytes per token.
    request.m_text = std::string(60000, 'x');

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_status == fcpp::ERequestStatus::Failed);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::RequestTooLong);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::Unselected);
    REQUIRE_FALSE(result.m_budget.m_ok);
    REQUIRE(result.m_message.find("does not fit") != std::string::npos);
    // Validation and the single terminal event, nothing in between.
    REQUIRE(result.m_trace.size() == 2);
    REQUIRE(result.m_trace.front() == fcpp::ETraceEvent::ValidationOk);
    REQUIRE(result.m_trace.back() == fcpp::ETraceEvent::ExitRequestTooLong);

    // No model was asked to do anything for a request that cannot be answered.
    REQUIRE(script.m_calls.empty());

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "request.finished") == 1);
    REQUIRE(log_text.find("REQUEST_TOO_LONG") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("file and history route attaches the bounded slice and the selected turns",
          "[orchestrator][route][file][history]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-file-history");
    const std::filesystem::path file = dir / "notes.md";
    write_file(file, "# Notes\n\nThis document describes the build steps.\n");

    SScript script;
    script.m_relevance = "YES";
    script.m_selection = "1";
    script.m_answer = "combined file answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const CFixedSymbolClang clang(std::nullopt);
    fcpp::CCursorContextAnalyzer analyzer(clang, llm);
    fcpp::CHistorian historian(llm);
    fcpp::CRequestOrchestrator orchestrator(logger, &historian, &analyzer, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-file-history";
    request.m_text = "Summarise the build steps and my earlier constraint";
    request.m_cursor_file = file.string();
    request.m_history = {{1, "The earlier constraint", "Keep it simple."}};

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::FileAndHistoryContextRequired);
    REQUIRE(result.m_message == "combined file answer");
    REQUIRE(result.m_history_context.size() == 1);
    REQUIRE_FALSE(result.m_file_context.empty());

    const std::string answer_prompt = script.last_text(fcpp::ELlmRole::CoderSmart);
    REQUIRE(answer_prompt.find("FILE CONTENT (bounded slice)") != std::string::npos);
    REQUIRE(answer_prompt.find("build steps") != std::string::npos);
    REQUIRE(answer_prompt.find("RELEVANT CONVERSATION HISTORY") != std::string::npos);
    REQUIRE(answer_prompt.find("The earlier constraint") != std::string::npos);

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "request.finished") == 1);
    REQUIRE(log_text.find("FILE_AND_HISTORY_CONTEXT_REQUIRED") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("confirmed file entity answers from facts without attaching the source",
          "[orchestrator][route][file][zero-window]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-file-facts");
    const std::filesystem::path file = dir / "widget.h";
    write_file(file,
               "namespace demo\n{\nclass CWidget\n{\npublic:\n"
               "    int hidden_impl_marker() const;\n};\n}\n");

    SScript script;
    script.m_relevance = "YES";
    script.m_entity = "ONE demo::CWidget";
    script.m_answer = "facts answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const CFixedSymbolClang clang(std::nullopt);
    fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    CFakeFactProvider facts;
    facts.m_sections = {{"Definition", "class CWidget { int hidden_impl_marker() const; };"}};
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm, &facts);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-file-facts";
    request.m_text = "What does CWidget declare?";
    request.m_cursor_file = file.string();

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::FileContextRequired);
    REQUIRE(result.m_file_entity == "demo::CWidget");
    REQUIRE(result.m_fact_sections.size() == 1);
    REQUIRE(result.m_message == "facts answer");

    const std::string answer_prompt = script.last_text(fcpp::ELlmRole::CoderSmart);
    REQUIRE(answer_prompt.find("PROJECT INDEX FACTS FOR demo::CWidget") != std::string::npos);
    // Zero-Window: the raw file is not duplicated on top of confirmed facts.
    REQUIRE(answer_prompt.find("FILE CONTENT (bounded slice)") == std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("entity rejected by the index falls back to the bounded file slice",
          "[orchestrator][route][file][zero-window]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-file-rejected");
    const std::filesystem::path file = dir / "widget.h";
    write_file(file, "namespace demo\n{\nclass CWidget\n{\n};\n}\n");

    SScript script;
    script.m_relevance = "YES";
    // The model names an entity that is not declared in this file.
    script.m_entity = "ONE demo::COther";
    script.m_answer = "slice answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const CFixedSymbolClang clang(std::nullopt);
    fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    CFakeFactProvider facts;
    facts.m_rejected = true;
    facts.m_sections = {{"Definition", "class COther;"}};
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm, &facts);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-file-rejected";
    request.m_text = "What is declared here?";
    request.m_cursor_file = file.string();

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::FileContextRequired);
    REQUIRE(result.m_file_entity.empty());
    REQUIRE(result.m_fact_sections.empty());
    REQUIRE(result.m_message == "slice answer");

    const std::string answer_prompt = script.last_text(fcpp::ELlmRole::CoderSmart);
    REQUIRE(answer_prompt.find("FILE CONTENT (bounded slice)") != std::string::npos);
    REQUIRE(answer_prompt.find("class CWidget") != std::string::npos);
    REQUIRE(answer_prompt.find("PROJECT INDEX FACTS") == std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("anonymous entity under the cursor skips facts but is reported in the log",
          "[orchestrator][route][cursor][anonymous]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-anonymous");
    const std::filesystem::path source = dir / "anon.cpp";
    write_file(source, "namespace\n{\nstruct\n{\n    int value;\n} instance;\n}\n");

    SScript script;
    script.m_relevance = "YES";
    script.m_answer = "anonymous answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    // An unnamed struct has a USR but no name: it cannot be an index subject.
    fcpp::SClangSymbol symbol{};
    symbol.m_usr = "c:@aN@S@anon";
    symbol.m_kind = fcpp::ESymbolKind::Struct;
    symbol.m_location.m_file = source.string();
    symbol.m_location.m_line = 3;
    symbol.m_location.m_column = 1;
    symbol.m_location.m_end_line = 6;
    symbol.m_is_definition = true;
    const CFixedSymbolClang clang(symbol);
    fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    CFakeFactProvider facts;
    facts.m_sections = {{"Definition", "should not be requested"}};
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm, &facts);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-anonymous";
    request.m_text = "What is this?";
    request.m_cursor_file = source.string();
    request.m_cursor_line = 3;
    request.m_cursor_column = 2;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    // A nameless entity is not an error: the request is still answerable.
    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::CursorContextRequired);
    REQUIRE(result.m_message == "anonymous answer");
    REQUIRE(result.m_fact_sections.empty());
    // The index was not asked about an entity it cannot be keyed by.
    REQUIRE(facts.m_calls == 0);

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "facts.skipped") == 1);
    REQUIRE(log_text.find("entity under cursor has no name") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("no contexts at all answers through the local assistant role",
          "[orchestrator][route][no-context]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-route-no-context");

    SScript script;
    script.m_answer = "generic answer";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, nullptr, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "route-no-context";
    request.m_text = "What is RAII?";

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::NoApplicableContext);
    REQUIRE(result.m_message == "generic answer");
    // One ThinkerLocal call to classify intent (default script: Answer),
    // one CoderLocal call for the answer itself.
    REQUIRE(script.m_calls.size() == 2);
    REQUIRE(result.m_intent == fcpp::EOrchestrationIntent::Answer);
    // The cheap local role answers when no project context is involved.
    REQUIRE(script.m_calls.back().first == fcpp::ELlmRole::CoderLocal);
    REQUIRE(script.m_calls.back().second.find("What is RAII?") != std::string::npos);

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "request.finished") == 1);
    REQUIRE(log_text.find("NO_APPLICABLE_CONTEXT") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("edit intent on cursor route applies a validated patch and skips "
          "build when allow_exec is NO",
          "[orchestrator][route][edit]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-edit-home");
    HomeScope scope(home);

    const std::filesystem::path dir = make_temp_dir("fcpp-edit-cursor");
    const std::filesystem::path source = dir / "widget.h";
    write_file(source, "class CWidget\n{\npublic:\n    int value() const;\n};\n");
    scaffold_project(dir, source);

    fcpp::CConfigManager config = make_config(home, /*allow_write=*/true, /*allow_exec=*/false);
    fcpp::CProjectManager project(config);
    REQUIRE(project.open(dir));

    SScript script;
    script.m_relevance = "YES";
    script.m_intent = "YES";
    script.m_patch =
        "--- a/widget.h\n"
        "+++ b/widget.h\n"
        "@@ -1,5 +1,6 @@\n"
        " class CWidget\n"
        " {\n"
        " public:\n"
        "+    int new_value() const;\n"
        "     int value() const;\n"
        " };\n";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const auto clang = make_clang(source, "CWidget");
    fcpp::CCursorContextAnalyzer analyzer(*clang, llm);

    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm,
        nullptr, &config, &project);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "edit-cursor";
    request.m_session_id = "edit-session";
    request.m_text = "Add a new_value() accessor next to value().";
    request.m_cursor_file = source.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_status == fcpp::ERequestStatus::Completed);
    REQUIRE(result.m_intent == fcpp::EOrchestrationIntent::Edit);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::None);
    REQUIRE(result.m_changed_files.size() == 1);
    REQUIRE(result.m_message.find("allow_exec is NO") != std::string::npos);

    // Cursor relevance (ThinkerLocal), intent (ThinkerLocal), patch (CoderSmart).
    REQUIRE(script.count_of(fcpp::ELlmRole::ThinkerLocal) == 2);
    REQUIRE(script.count_of(fcpp::ELlmRole::CoderSmart) == 1);

    const std::string patch_prompt = script.last_text(fcpp::ELlmRole::CoderSmart);
    REQUIRE(patch_prompt.find("Add a new_value() accessor") != std::string::npos);

    // The patch actually landed on disk.
    std::ifstream patched(source);
    const std::string content((std::istreambuf_iterator<char>(patched)),
                              std::istreambuf_iterator<char>());
    REQUIRE(content.find("new_value") != std::string::npos);

    const std::string log_text = read_log(logger);
    REQUIRE(count_events(log_text, "patch.generated") == 1);
    REQUIRE(count_events(log_text, "patch.validated") == 1);
    REQUIRE(count_events(log_text, "patch.applied") == 1);
    // No build was attempted: allow_exec is NO.
    REQUIRE(count_events(log_text, "patch.build") == 0);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::remove_all(home, ignored);
}

TEST_CASE("edit intent is refused by configuration when allow_write is NO",
          "[orchestrator][route][edit]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-edit-denied-home");
    HomeScope scope(home);

    const std::filesystem::path dir = make_temp_dir("fcpp-edit-denied");
    const std::filesystem::path source = dir / "widget.h";
    write_file(source, "class CWidget\n{\npublic:\n    int value() const;\n};\n");
    scaffold_project(dir, source);

    fcpp::CConfigManager config = make_config(home, /*allow_write=*/false, /*allow_exec=*/true);
    fcpp::CProjectManager project(config);
    REQUIRE(project.open(dir));

    SScript script;
    script.m_relevance = "YES";
    script.m_intent = "YES";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const auto clang = make_clang(source, "CWidget");
    fcpp::CCursorContextAnalyzer analyzer(*clang, llm);

    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm,
        nullptr, &config, &project);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "edit-denied";
    request.m_text = "Rename value() to get_value().";
    request.m_cursor_file = source.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::PermissionDenied);
    REQUIRE(result.m_trace.back() == fcpp::ETraceEvent::ExitPermissionDenied);
    REQUIRE(result.m_changed_files.empty());

    // Permission is checked before the model is ever asked for a patch.
    REQUIRE(script.count_of(fcpp::ELlmRole::CoderSmart) == 0);

    // The source file is untouched.
    std::ifstream unchanged(source);
    const std::string content((std::istreambuf_iterator<char>(unchanged)),
                              std::istreambuf_iterator<char>());
    REQUIRE(content.find("get_value") == std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::remove_all(home, ignored);
}

TEST_CASE("edit intent with no file or cursor target is refused before any "
          "model call for a patch",
          "[orchestrator][route][edit]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-edit-no-target");

    SScript script;
    script.m_intent = "YES";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    // No config/project manager wired either — same refusal either way.
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, nullptr, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "edit-no-target";
    request.m_text = "Fix the bug in this module.";

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::NoApplicableContext);
    REQUIRE(result.m_intent == fcpp::EOrchestrationIntent::Edit);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::EditWithoutTarget);
    REQUIRE(result.m_trace.back() == fcpp::ETraceEvent::ExitEditWithoutTarget);

    // Only the intent classification itself was asked; no CoderLocal/CoderSmart call.
    REQUIRE(script.count_of(fcpp::ELlmRole::CoderLocal) == 0);
    REQUIRE(script.count_of(fcpp::ELlmRole::CoderSmart) == 0);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("model refusing to produce a diff ends the edit visibly, not with "
          "a mutated file",
          "[orchestrator][route][edit]")
{
    const std::filesystem::path home = make_temp_dir("fcpp-edit-garbage-home");
    HomeScope scope(home);

    const std::filesystem::path dir = make_temp_dir("fcpp-edit-garbage");
    const std::filesystem::path source = dir / "widget.h";
    write_file(source, "class CWidget\n{\npublic:\n    int value() const;\n};\n");
    scaffold_project(dir, source);

    fcpp::CConfigManager config = make_config(home, /*allow_write=*/true, /*allow_exec=*/true);
    fcpp::CProjectManager project(config);
    REQUIRE(project.open(dir));

    SScript script;
    script.m_relevance = "YES";
    script.m_intent = "YES";
    // No "@@"/"---"/"+++" at all: caught by CLlmLayer's own cheap sanity gate
    // for CppPatch (validate_prompt_result) before CPatchParser ever runs —
    // surfaces as LlmGenerationFailed, not PatchUnparsable.
    script.m_patch = "Sure, here is what I would change: just rename the method.";

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Trace);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [&script](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CScriptedModel>(role, script);
        });

    const auto clang = make_clang(source, "CWidget");
    fcpp::CCursorContextAnalyzer analyzer(*clang, llm);

    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm,
        nullptr, &config, &project);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "edit-garbage";
    request.m_text = "Rename value() to get_value().";
    request.m_cursor_file = source.string();
    request.m_cursor_line = 2;
    request.m_cursor_column = 3;

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_exit_reason == fcpp::EExitReason::LlmGenerationFailed);
    REQUIRE(result.m_trace.back() == fcpp::ETraceEvent::ExitLlmGenerationFailed);
    REQUIRE(result.m_changed_files.empty());

    std::ifstream unchanged(source);
    const std::string content((std::istreambuf_iterator<char>(unchanged)),
                              std::istreambuf_iterator<char>());
    REQUIRE(content.find("get_value") == std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::remove_all(home, ignored);
}
