/**
 * @file CAgentApi.cpp
 * @brief Реализация Agent API (§89, §93).
 */

#include "core/CAgentApi.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "core/CAskService.h"
#include "core/CLogger.h"
#include "core/CHash.h"
#include "core/CDoctor.h"
#include "core/CPatchApplier.h"
#include "core/CPatchParser.h"
#include "core/CPatchValidator.h"
#include "core/CExecutor.h"
#include "fcpp-agent/IClang.h"
#include "index/CClangFactory.h"
#include "tools/CToolContext.h"
#include "tools/CToolRegistry.h"

namespace fcpp
{

namespace
{

/** @brief Имена методов API, индексированные по EApiMethod (Секция 3). */
static const char* const m_api_method_names[static_cast<std::size_t>(EApiMethod::Count)]
{
    "agent.status",
    "agent.capabilities",
    "config.reload",
    "project.open",
    "project.close",
    "project.status",
    "project.list",
    "index.status",
    "index.refresh",
    "index.rebuild",
    "build.build",
    "tests.run",
    "tool",
    "patch",
    "doctor",
    "ask",
    "session.open",
    "session.close",
    "session.update_context",
    "project.profile.set",
    "question.shrink"
};

/**
 * @brief Сформировать уникальный идентификатор conversation session.
 * @return Непрозрачный идентификатор.
 */
std::string make_session_id()
{
    static std::atomic<std::uint64_t> counter{0};
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string seed = std::to_string(ticks) + ":" + std::to_string(counter.fetch_add(1));
    return "session-" + CHash::sha256_hex(seed).substr(0, 24);
}

/**
 * @brief Сформировать ответ-ошибку.
 * @param kind Категория ошибки.
 * @param message Сообщение.
 * @return Ответ.
 */
SApiResponse make_error(EErrorKind kind, const std::string& message)
{
    SApiResponse response;
    response.m_ok = false;
    response.m_error = kind;
    response.m_message = message;
    return response;
}


/** Parse the current deterministic clarification format into a public question DTO. */
SChatQuestion make_chat_question(const std::string& text, std::uint16_t question_id)
{
    SChatQuestion result;
    SQuestion question;
    question.m_id = question_id;

    std::size_t line_start = 0;
    bool first_line = true;
    while (line_start <= text.size())
    {
        const std::size_t line_end = text.find('\n', line_start);
        std::string line = text.substr(
            line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
        {
            line.pop_back();
        }
        const std::size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos)
        {
            line.erase(0, first);
            if (first_line)
            {
                question.m_text = line;
                first_line = false;
            }
            else if (line.rfind("- ", 0) == 0)
            {
                std::string option = line.substr(2);
                const std::size_t option_start = option.find_first_not_of(" \t");
                if (option_start != std::string::npos)
                {
                    option.erase(0, option_start);
                    const auto normalize_option = [](const std::string& value)
                    {
                        std::string result;
                        bool previous_space = false;
                        for (const unsigned char ch : value)
                        {
                            if (std::isspace(ch))
                            {
                                if (!result.empty() && !previous_space)
                                {
                                    result.push_back(' ');
                                }
                                previous_space = true;
                            }
                            else
                            {
                                result.push_back(static_cast<char>(std::tolower(ch)));
                                previous_space = false;
                            }
                        }
                        if (!result.empty() && result.back() == ' ')
                        {
                            result.pop_back();
                        }
                        return result;
                    };
                    const std::string normalized_option = normalize_option(option);
                    const bool duplicate = std::any_of(
                        question.m_options.begin(), question.m_options.end(),
                        [&normalize_option, &normalized_option](const std::string& existing)
                        {
                            return normalize_option(existing) == normalized_option;
                        });
                    if (!duplicate)
                    {
                        question.m_options.push_back(std::move(option));
                    }
                }
            }
            else
            {
                question.m_text += "\n" + line;
            }
        }
        if (line_end == std::string::npos)
        {
            break;
        }
        line_start = line_end + 1;
    }

    if (question.m_text.empty())
    {
        question.m_text = text;
    }
    result.m_questions.push_back(std::move(question));
    result.m_wait_for_user = true;
    return result;
}

nlohmann::json chat_question_json(const SChatQuestion& question)
{
    nlohmann::json result = {
        {"wait_for_user", question.m_wait_for_user},
        {"questions", nlohmann::json::array()}
    };
    for (const SQuestion& item : question.m_questions)
    {
        result["questions"].push_back({
            {"id", item.m_id},
            {"text", item.m_text},
            {"options", item.m_options},
            {"and", item.m_and}
        });
    }
    return result;
}

std::uint16_t next_question_id(const std::vector<SConversationEvent>& events)
{
    std::uint32_t maximum = 0;
    for (const SConversationEvent& event : events)
    {
        if (event.m_kind != "assistant")
        {
            continue;
        }
        try
        {
            const nlohmann::json payload = nlohmann::json::parse(event.m_payload);
            if (!payload.contains("question") || !payload["question"].is_object()
                || !payload["question"].contains("questions")
                || !payload["question"]["questions"].is_array())
            {
                continue;
            }
            for (const nlohmann::json& item : payload["question"]["questions"])
            {
                if (item.is_object() && item.contains("id") && item["id"].is_number_integer())
                {
                    const std::int64_t id = item["id"].get<std::int64_t>();
                    if (id > 0 && id <= UINT16_MAX)
                    {
                        maximum = std::max(maximum, static_cast<std::uint32_t>(id));
                    }
                }
            }
        }
        catch (const nlohmann::json::exception&)
        {
            continue;
        }
    }
    return maximum >= UINT16_MAX ? 0 : static_cast<std::uint16_t>(maximum + 1);
}

bool question_has_option(const std::vector<SConversationEvent>& events,
                         std::uint16_t question_id,
                         const std::string& option)
{
    const std::vector<SConversationEvent> active_events = active_conversation_events(events);
    for (auto event = active_events.rbegin(); event != active_events.rend(); ++event)
    {
        if (event->m_kind != "assistant")
        {
            continue;
        }
        try
        {
            const nlohmann::json payload = nlohmann::json::parse(event->m_payload);
            if (!payload.contains("question") || !payload["question"].is_object()
                || !payload["question"].contains("questions")
                || !payload["question"]["questions"].is_array())
            {
                continue;
            }
            for (const nlohmann::json& item : payload["question"]["questions"])
            {
                if (!item.is_object() || !item.contains("id") || !item["id"].is_number_integer()
                    || item["id"].get<std::int64_t>() != question_id
                    || !item.contains("options") || !item["options"].is_array())
                {
                    continue;
                }
                for (const nlohmann::json& candidate : item["options"])
                {
                    if (candidate.is_string() && candidate.get<std::string>() == option)
                    {
                        return true;
                    }
                }
            }
        }
        catch (const nlohmann::json::exception&)
        {
            continue;
        }
    }
    return false;
}

} // namespace

const char* api_method_name(EApiMethod method)
{
    return m_api_method_names[static_cast<std::size_t>(method)];
}

bool api_is_tool_command(const std::string& text)
{
    for (std::size_t index = 0; index < static_cast<std::size_t>(EToolName::Count); ++index)
    {
        if (text == tool_name(static_cast<EToolName>(index)))
        {
            return true;
        }
    }
    return false;
}

CAgentApi::CAgentApi(const CConfigManager& config_manager)
    : m_config_manager(config_manager)
    , m_project_manager(config_manager)
    , m_conversation_store()
    , m_logger(config_manager.agent_dir() / "logs", ELogLevel::Info)
{
    m_conversation_store.open(m_config_manager.agent_dir() / "sessions.sqlite");
}

SApiResponse CAgentApi::execute(const SApiRequest& request)
{
    static std::atomic<std::uint64_t> request_counter{0};
    const auto started = std::chrono::steady_clock::now();
    std::string request_id = request.m_request_id;
    if (request_id.empty())
    {
        const std::string seed = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count())
            + ":" + std::to_string(request_counter.fetch_add(1));
        request_id = "agent-" + CHash::sha256_hex(seed).substr(0, 16);
    }
    m_logger.log_event(ELogLevel::Info, "request.start", {
        {"request_id", request_id}, {"method", api_method_name(request.m_method)},
        {"session_id", request.m_session_id}, {"text_bytes", request.m_text.size()},
        {"has_editor", !request.m_editor_file.empty()}, {"stream", request.m_stream}
    });

    SApiRequest traced_request = request;
    traced_request.m_request_id = request_id;
    SApiResponse response = execute_impl(traced_request);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    m_logger.log_event(response.m_ok ? ELogLevel::Info : ELogLevel::Warn, "request.finish", {
        {"request_id", request_id}, {"method", api_method_name(request.m_method)},
        {"ok", response.m_ok}, {"error_kind", static_cast<int>(response.m_error)},
        {"state", response.m_state}, {"duration_ms", elapsed},
        {"session_id", response.m_session_id.empty() ? request.m_session_id : response.m_session_id}
    });
    return response;
}

SApiResponse CAgentApi::execute_impl(const SApiRequest& request)
{
    SApiResponse response;

    switch (request.m_method)
    {
        case EApiMethod::AgentStatus:
        {
            response.m_ok = true;
            response.m_message = "READY";
            return response;
        }
        case EApiMethod::AgentCapabilities:
        {
            response.m_ok = true;
            for (std::size_t index = 0; index < static_cast<std::size_t>(EApiMethod::Count); ++index)
            {
                response.m_names.push_back(api_method_name(static_cast<EApiMethod>(index)));
            }
            return response;
        }
        case EApiMethod::ConfigReload:
        {
            if (!m_config_manager.reload_config())
            {
                return make_error(EErrorKind::IoError, "cannot reload agent.conf");
            }
            response.m_ok = true;
            response.m_message = "configuration reloaded";
            return response;
        }
        case EApiMethod::ProjectOpen:
        {
            // Шаг протокола `open`: корень проекта приходит как `projectRoot`
            // (m_project_root) либо как `m_path` (CLI). Приоритет — projectRoot.
            const std::string& target =
                !request.m_project_root.empty() ? request.m_project_root : request.m_path;
            std::string failure_stage;
            if (target.empty() || !m_project_manager.open(target, &failure_stage))
            {
                if (target.empty())
                {
                    failure_stage = "missing_project_root";
                }
                m_logger.log_event(ELogLevel::Warn, "project.open.failed", {
                    {"request_id", request.m_request_id},
                    {"failure_stage", failure_stage},
                    {"root_provided", !target.empty()}
                });
                return make_error(EErrorKind::InvalidRequest,
                                  "cannot open project (stage: " + failure_stage + ")");
            }
            m_logger.log_event(ELogLevel::Info, "project.open.succeeded", {
                {"request_id", request.m_request_id},
                {"project_id", m_project_manager.current().m_project.m_project_id},
                {"index_state", static_cast<int>(m_project_manager.current().m_index_status.m_state)},
                {"index_files", m_project_manager.current().m_index_status.m_file_count},
                {"index_symbols", m_project_manager.current().m_index_status.m_symbol_count}
            });
            m_project_manager.save_session();
            response.m_ok = true;
            response.m_state = "READY";
            response.m_project_id = m_project_manager.current().m_project.m_project_id;
            response.m_index_status = m_project_manager.current().m_index_status;
            return response;
        }
        case EApiMethod::ProjectClose:
        {
            m_project_manager.close();
            m_project_manager.clear_session();
            response.m_ok = true;
            return response;
        }
        case EApiMethod::ProjectStatus:
        {
            // Read-only: лёгкое восстановление (без open/warm/refresh).
            if (!m_project_manager.has_current() && !m_project_manager.restore_session_light())
            {
                return make_error(EErrorKind::ProjectNotOpen, "project not open");
            }
            if (!m_project_manager.has_current())
            {
                return make_error(EErrorKind::ProjectNotOpen, "project not open");
            }
            response.m_ok = true;
            response.m_project_id = m_project_manager.current().m_project.m_project_id;
            response.m_index_status = m_project_manager.current().m_index_status;
            return response;
        }
        case EApiMethod::ProjectList:
        {
            response.m_ok = true;
            for (const SProject& project : m_project_manager.list())
            {
                response.m_names.push_back(project.m_project_id + "  " + project.m_canonical_path.string());
            }
            return response;
        }
        case EApiMethod::IndexStatus:
        {
            // Read-only: лёгкое восстановление (без open/warm/refresh).
            if (!m_project_manager.has_current())
            {
                m_project_manager.restore_session_light();
            }
            if (!m_project_manager.has_current())
            {
                return make_error(EErrorKind::ProjectNotOpen, "project not open");
            }
            response.m_ok = true;
            response.m_index_status = m_project_manager.current().m_index_status;
            return response;
        }
        case EApiMethod::IndexRefresh:
        {
            if (!m_project_manager.is_opened())
            {
                m_project_manager.restore_session();
            }
            if (!m_project_manager.is_opened() || !m_project_manager.refresh_index())
            {
                return make_error(EErrorKind::IndexNotReady, "index refresh failed");
            }
            response.m_ok = true;
            response.m_index_status = m_project_manager.current().m_index_status;
            return response;
        }
        case EApiMethod::IndexRebuild:
        {
            if (!m_project_manager.is_opened())
            {
                m_project_manager.restore_session();
            }
            if (!m_project_manager.is_opened() || !m_project_manager.rebuild_index())
            {
                return make_error(EErrorKind::IndexNotReady, "index rebuild failed");
            }
            response.m_ok = true;
            response.m_index_status = m_project_manager.current().m_index_status;
            return response;
        }
        case EApiMethod::Build:
        case EApiMethod::Tests:
        {
            if (!m_project_manager.is_opened())
            {
                m_project_manager.restore_session();
            }
            if (!m_project_manager.is_opened())
            {
                return make_error(EErrorKind::ProjectNotOpen, "project not fully open");
            }
            if (!m_config_manager.config().m_allow_exec)
            {
                return make_error(EErrorKind::PermissionDenied, "allow_exec is NO");
            }

            const std::filesystem::path build_dir =
                m_project_manager.current().m_project.m_canonical_path / m_config_manager.config().m_default_build;

            CExecutor executor;
            SCommandArgs args;
            if (request.m_method == EApiMethod::Build)
            {
                args.m_tool = m_config_manager.config().m_cmake_path.empty()
                                  ? "cmake"
                                  : m_config_manager.config().m_cmake_path.string();
                args.m_arguments = {"--build", build_dir.string()};
            }
            else
            {
                args.m_tool = "ctest";
                args.m_arguments = {"--test-dir", build_dir.string(), "--output-on-failure"};
            }
            args.m_working_dir = m_project_manager.current().m_project.m_canonical_path;
            args.m_effect = EFFECT_WRITE_BUILD;
            args.m_timeout_seconds = 600;

            const SExecutionResult result = executor.execute(args, nullptr);
            response.m_text = result.m_stdout;
            if (!result.m_allowed || result.m_exit_code != 0)
            {
                response.m_ok = false;
                response.m_error = EErrorKind::IoError;
                response.m_message = "command failed (exit " + std::to_string(result.m_exit_code) + ")\n" + result.m_stderr;
                return response;
            }
            response.m_ok = true;
            return response;
        }
        case EApiMethod::Tool:
            return handle_tool(request);
        case EApiMethod::Patch:
            return handle_patch(request);
        case EApiMethod::Doctor:
        {
            response.m_ok = CDoctor().run(m_config_manager.config());
            return response;
        }
        case EApiMethod::Ask:
            return handle_ask(request);
        case EApiMethod::SessionOpen:
        {
            const std::string session_id = make_session_id();
            const std::string project_id = m_project_manager.has_current()
                ? m_project_manager.current().m_project.m_project_id : std::string();
            nlohmann::json payload = {{"project_id", project_id}};
            if (!m_conversation_store.create_session_with_event(
                    session_id, project_id, "session.open", payload.dump()))
            {
                return make_error(EErrorKind::IoError, "cannot create conversation session");
            }
            response.m_ok = true;
            response.m_state = "READY";
            response.m_session_id = session_id;
            response.m_project_id = project_id;
            return response;
        }
        case EApiMethod::SessionClose:
        {
            if (request.m_session_id.empty())
            {
                return make_error(EErrorKind::InvalidRequest, "missing session_id");
            }
            if (!m_conversation_store.close_session_with_event(
                    request.m_session_id, "session.close", "{}"))
            {
                return make_error(EErrorKind::InvalidRequest, "unknown, closed, or uncloseable session");
            }
            response.m_ok = true;
            response.m_session_id = request.m_session_id;
            return response;
        }
        case EApiMethod::QuestionShrink:
        {
            if (request.m_session_id.empty() || request.m_question_id == 0)
            {
                return make_error(EErrorKind::InvalidRequest,
                                  "session_id and non-zero question_id are required");
            }
            const auto session = m_conversation_store.get_session(request.m_session_id);
            if (!session.has_value() || session->m_closed)
            {
                return make_error(EErrorKind::InvalidRequest, "unknown or closed conversation session");
            }
            response.m_ok = true;
            response.m_state = "COMPLETED";
            response.m_has_shrink_result = true;
            response.m_shrink_result = m_conversation_store.shrink_question(
                request.m_session_id, request.m_question_id);
            response.m_message = response.m_shrink_result
                ? "conversation truncated" : "conversation unchanged";
            return response;
        }
        case EApiMethod::ProjectProfileSet:
        {
            if (!m_project_manager.has_current())
            {
                return make_error(EErrorKind::ProjectNotOpen,
                                  "project not open; call open before setting project profile");
            }
            if (!m_project_manager.set_profile(request.m_profile_text))
            {
                return make_error(EErrorKind::IoError, "cannot persist project profile");
            }
            response.m_ok = true;
            response.m_state = "COMPLETED";
            response.m_project_id = m_project_manager.current().m_project.m_project_id;
            response.m_message = "project profile set";
            return response;
        }
        case EApiMethod::SessionUpdateContext:
        {
            if (request.m_session_id.empty() || request.m_context_json.empty())
            {
                return make_error(EErrorKind::InvalidRequest, "session_id and context are required");
            }
            try
            {
                (void)nlohmann::json::parse(request.m_context_json);
            }
            catch (const nlohmann::json::exception&)
            {
                return make_error(EErrorKind::InvalidRequest, "context must be valid JSON");
            }
            if (!m_conversation_store.update_context_with_event(
                    request.m_session_id,
                    request.m_context_json,
                    "session.context_updated",
                    request.m_context_json))
            {
                return make_error(EErrorKind::IoError,
                                  "cannot atomically persist conversation context update");
            }
            response.m_ok = true;
            response.m_session_id = request.m_session_id;
            return response;
        }
        default:
            return make_error(EErrorKind::InvalidRequest, "unknown method");
    }
}

SApiResponse CAgentApi::handle_ask(const SApiRequest& request)
{
    const bool answering_choice = !request.m_selected_option.empty();
    if ((!answering_choice && request.m_text.empty())
        || (answering_choice && (request.m_session_id.empty() || request.m_answer_question_id == 0)))
    {
        return make_error(EErrorKind::InvalidRequest,
                          answering_choice
                              ? "session_id and answer.question_id are required for a choice"
                              : "empty ask text");
    }

    const std::string effective_text = answering_choice ? request.m_selected_option : request.m_text;

    const bool had_light_project = m_project_manager.has_current()
        && !m_project_manager.is_opened();
    if (!m_project_manager.is_opened())
    {
        m_project_manager.restore_session();
    }
    if (had_light_project && !m_project_manager.is_opened())
    {
        return make_error(EErrorKind::IndexNotReady,
                          "project context is known but full project open failed");
    }

    std::string session_context_json = "{}";
    std::vector<SConversationTurn> conversation_history;
    std::vector<SConversationEvent> stored_events;
    if (!request.m_session_id.empty())
    {
        const auto session = m_conversation_store.get_session(request.m_session_id);
        if (!session.has_value() || session->m_closed)
        {
            return make_error(EErrorKind::InvalidRequest, "unknown or closed conversation session");
        }
        if (!session->m_project_id.empty() && m_project_manager.has_current()
            && session->m_project_id != m_project_manager.current().m_project.m_project_id)
        {
            return make_error(EErrorKind::InvalidRequest, "conversation session belongs to another project");
        }

        session_context_json = session->m_context_json;
        std::int64_t after_event_id = 0;
        constexpr std::size_t k_event_page_size = 1000;
        while (true)
        {
            std::vector<SConversationEvent> page = m_conversation_store.events(
                request.m_session_id, after_event_id, k_event_page_size);
            if (page.empty())
            {
                break;
            }
            after_event_id = page.back().m_id;
            stored_events.insert(stored_events.end(), page.begin(), page.end());
            if (page.size() < k_event_page_size)
            {
                break;
            }
        }

        if (answering_choice
            && !question_has_option(stored_events, request.m_answer_question_id, request.m_selected_option))
        {
            return make_error(EErrorKind::InvalidRequest,
                              "question_id or selected option does not match conversation history");
        }

        conversation_history = reconstruct_conversation_turns(stored_events);
        nlohmann::json user_event = {
            {"text", effective_text},
            {"editor_file", request.m_editor_file},
            {"editor_line", request.m_editor_line},
            {"editor_column", request.m_editor_column}
        };
        if (answering_choice)
        {
            user_event["answer_question_id"] = request.m_answer_question_id;
            user_event["selected_option"] = request.m_selected_option;
        }
        if (!m_conversation_store.append_event(request.m_session_id, "user", user_event.dump()))
        {
            return make_error(EErrorKind::IoError, "cannot persist user message");
        }
    }

    CAskService service(m_config_manager, m_project_manager, &m_logger, request.m_request_id);
    SApiResponse response;
    std::string answer;
    response.m_ok = service.ask(effective_text,
                                request.m_stream,
                                request.m_editor_file,
                                request.m_editor_line,
                                request.m_editor_column,
                                answer,
                                session_context_json,
                                conversation_history,
                                request.m_on_stream_chunk);

    const bool clarification = !response.m_ok
        && answer.rfind("Уточните, что имеется в виду:", 0) == 0;
    if (clarification)
    {
        const std::uint16_t question_id = request.m_session_id.empty()
            ? 1 : next_question_id(stored_events);
        if (question_id == 0)
        {
            return make_error(EErrorKind::IoError, "question ID space exhausted");
        }
        response.m_ok = true;
        response.m_state = "CLARIFY";
        response.m_has_question = true;
        response.m_question = make_chat_question(answer, question_id);
    }

    if (!request.m_session_id.empty())
    {
        nlohmann::json assistant_event = {
            {"text", answer},
            {"ok", response.m_ok}
        };
        if (response.m_has_question)
        {
            assistant_event["question"] = chat_question_json(response.m_question);
        }
        if (!m_conversation_store.append_event(request.m_session_id, "assistant", assistant_event.dump()))
        {
            return make_error(EErrorKind::IoError, "answer produced but could not persist conversation event");
        }
        response.m_session_id = request.m_session_id;
    }
    if (response.m_ok && !response.m_has_question)
    {
        response.m_state = "COMPLETED";
        response.m_text = answer;
    }
    else if (!response.m_ok && !answer.empty())
    {
        response.m_error = EErrorKind::Ambiguous;
        response.m_message = answer;
    }
    return response;
}

SApiResponse CAgentApi::handle_tool(const SApiRequest& request)
{
    if (!m_project_manager.has_current())
    {
        m_project_manager.restore_session();
    }
    if (!m_project_manager.has_current())
    {
        return make_error(EErrorKind::ProjectNotOpen, "project not open");
    }

    EToolName tool;
    if (!CToolRegistry::parse_tool_name(request.m_tool, tool))
    {
        return make_error(EErrorKind::InvalidRequest, "unknown tool: " + request.m_tool);
    }

    const SProjectContext& context = m_project_manager.current();
    std::unique_ptr<IClang> clang = create_clang_backend(m_config_manager.config().m_clang_path);

    CToolContext tool_context(context.m_project.m_db_path, context.m_project.m_canonical_path, clang.get());
    if (!tool_context.open())
    {
        return make_error(EErrorKind::IndexNotReady, "cannot open project index");
    }

    SToolRequest tool_request;
    tool_request.m_tool = tool;
    tool_request.m_target = request.m_target;
    tool_request.m_limit = request.m_limit;

    const CToolRegistry registry;
    const SToolResult tool_result = registry.execute(tool, tool_request, tool_context);

    SApiResponse response;
    response.m_ok = tool_result.m_ok;
    response.m_error = tool_result.m_error;
    response.m_message = tool_result.m_message;
    response.m_symbols = tool_result.m_symbols;
    response.m_names = tool_result.m_names;
    response.m_locations = tool_result.m_locations;
    response.m_text = tool_result.m_text;
    return response;
}

SApiResponse CAgentApi::handle_patch(const SApiRequest& request)
{
    if (!m_project_manager.has_current())
    {
        m_project_manager.restore_session();
    }
    if (!m_project_manager.has_current())
    {
        return make_error(EErrorKind::ProjectNotOpen, "project not open");
    }
    if (!m_config_manager.config().m_allow_write)
    {
        return make_error(EErrorKind::PermissionDenied, "allow_write is NO");
    }

    SPatchSet patch;
    if (!CPatchParser::parse(request.m_payload, patch))
    {
        return make_error(EErrorKind::InvalidRequest, "cannot parse patch");
    }

    CPatchValidator validator(m_project_manager.current().m_project.m_canonical_path);
    SPatchValidation validation;
    if (!validator.validate(patch, validation))
    {
        return make_error(EErrorKind::PatchInvalid, validation.m_message);
    }

    CPatchApplier applier;
    std::string message;
    if (!applier.apply(validation.m_changes, message))
    {
        return make_error(EErrorKind::PatchApplyFailed, message);
    }

    // COMPILATION FAIL (детерминированный триггер ADR-10): при провале сборки — откат.
    if (request.m_build_after)
    {
        if (!m_config_manager.config().m_allow_exec)
        {
            applier.revert(validation.m_changes, message);
            return make_error(EErrorKind::PermissionDenied, "allow_exec is NO; patch reverted");
        }

        const std::filesystem::path build_dir =
            m_project_manager.current().m_project.m_canonical_path / m_config_manager.config().m_default_build;

        CExecutor executor;
        SCommandArgs args;
        args.m_tool = m_config_manager.config().m_cmake_path.empty()
                          ? "cmake"
                          : m_config_manager.config().m_cmake_path.string();
        args.m_arguments = {"--build", build_dir.string()};
        args.m_working_dir = m_project_manager.current().m_project.m_canonical_path;
        args.m_effect = EFFECT_WRITE_BUILD;
        args.m_timeout_seconds = 600;

        const SExecutionResult build = executor.execute(args, nullptr);
        if (!build.m_allowed || build.m_exit_code != 0)
        {
            std::string revert_message;
            applier.revert(validation.m_changes, revert_message);
            SApiResponse failed;
            failed.m_ok = false;
            failed.m_error = EErrorKind::BuildFailed;
            failed.m_message = "COMPILATION FAIL; patch reverted\n" + build.m_stderr;
            return failed;
        }
    }

    SApiResponse response;
    response.m_ok = true;
    response.m_text = "patch applied (" + std::to_string(validation.m_changes.size()) + " files)";
    return response;
}

} // namespace fcpp
