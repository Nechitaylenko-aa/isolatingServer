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
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "core/CRequestOrchestrator.h"
#include "core/CIndexFactProvider.h"
#include "core/CHistorian.h"
#include "core/CCursorContextAnalyzer.h"
#include "llm/CLlmLayer.h"
#include "core/CLogger.h"
#include "core/CHash.h"
#include "core/CDoctor.h"
#include "core/CExecutor.h"
#include "core/CGitSnapshotStore.h"
#include "fcpp-agent/IClang.h"
#include "index/CClangFactory.h"
#include "index/CSQLiteIndex.h"
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
    "doctor",
    "ask",
    "edit.rollback",
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


/**
 * @brief Отобразить решение дерева (EExitReason) в категорию ошибки API.
 *
 * Дерево уже приняло точное, типизированное решение о причине отказа —
 * граница API должна его читать, а не угадывать заново по содержимому
 * m_message (прежняя логика так и делала: answer.empty() ? NotUnderstood :
 * Ambiguous — все 15 причин derevа схлопывались в один и тот же Ambiguous).
 *
 * @param reason Причина завершения из SOrchestrationResult.
 * @return Ближайшая по смыслу категория EErrorKind.
 */
EErrorKind error_kind_for_exit_reason(EExitReason reason)
{
    switch (reason)
    {
    case EExitReason::None:
        return EErrorKind::Ok;
    case EExitReason::EmptyRequest:
    case EExitReason::RequestTooLong:
    case EExitReason::EditWithoutTarget:
        return EErrorKind::InvalidRequest;
    case EExitReason::CursorRelevanceFailed:
    case EExitReason::HistoryRelevanceFailed:
    case EExitReason::FileRelevanceFailed:
    case EExitReason::FileNotRelevant:
        return EErrorKind::NotUnderstood;
    case EExitReason::LlmGenerationFailed:
        return EErrorKind::IoError;
    case EExitReason::IndexUnavailable:
        return EErrorKind::IndexNotReady;
    case EExitReason::PermissionDenied:
        return EErrorKind::PermissionDenied;
    case EExitReason::PatchUnparsable:
    case EExitReason::PatchInvalid:
        return EErrorKind::PatchInvalid;
    case EExitReason::PatchApplyFailed:
        return EErrorKind::PatchApplyFailed;
    case EExitReason::BuildFailed:
        return EErrorKind::BuildFailed;
    case EExitReason::Count:
        break;
    }
    return EErrorKind::NotUnderstood;
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
    // Адрес диалога эхом возвращается в любом исходе: клиент не должен терять
    // session_id из-за отказа, иначе он не может связать ответ с историей.
    if (response.m_session_id.empty())
    {
        response.m_session_id = request.m_session_id;
    }
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
            bool opened = false;

            if (target.empty())
            {
                failure_stage = "missing_project_root";
            }
            else
            {
                // Empty directories are outside the supported initialization
                // scope for now. Reject explicitly instead of treating this as
                // an opaque CMake/project-manager failure.
                std::error_code directory_error;
                const std::filesystem::path target_path(target);
                if (std::filesystem::is_directory(target_path, directory_error)
                    && !directory_error)
                {
                    const bool empty = std::filesystem::is_empty(target_path, directory_error);
                    if (directory_error)
                    {
                        failure_stage = "inspect_project_directory";
                    }
                    else if (empty)
                    {
                        failure_stage = "empty_project_directory";
                    }
                }

                if (failure_stage.empty())
                {
                    opened = m_project_manager.open(target, &failure_stage);
                }
            }

            if (!opened)
            {
                if (failure_stage.empty())
                {
                    failure_stage = "project_open_failed";
                }
                // Directory name only: the journal must not contain absolute
                // paths (plugin_api.md). The stage locates the failure.
                m_logger.log_event(ELogLevel::Warn, "project.open.failed", {
                    {"request_id", request.m_request_id},
                    {"failure_stage", failure_stage},
                    {"project_dir", target.empty()
                        ? std::string()
                        : std::filesystem::path(target).filename().string()},
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

            // «Проект открыт» = «проект адресуем». Клиент не должен угадывать
            // необходимость session.open: для нового проекта адрес диалога
            // создаётся здесь, для существующего — возвращается активный.
            const std::string project_id = m_project_manager.current().m_project.m_project_id;
            std::string session_id;
            const auto existing = m_conversation_store.latest_session_for_project(project_id);
            if (existing.has_value())
            {
                session_id = existing->m_id;
            }
            else
            {
                session_id = make_session_id();
                nlohmann::json payload = {{"project_id", project_id}};
                if (!m_conversation_store.create_session_with_event(
                        session_id, project_id, "session.open", payload.dump()))
                {
                    m_logger.log_event(ELogLevel::Warn, "project.open.failed", {
                        {"request_id", request.m_request_id},
                        {"failure_stage", "create_conversation_session"},
                        {"root_provided", true}
                    });
                    return make_error(EErrorKind::IoError,
                                      "cannot create conversation session for opened project");
                }
            }

            response.m_ok = true;
            response.m_state = "READY";
            response.m_project_id = project_id;
            response.m_session_id = session_id;
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
        case EApiMethod::Doctor:
        {
            response.m_ok = CDoctor().run(m_config_manager.config());
            return response;
        }
        case EApiMethod::Ask:
            return handle_ask(request);
        case EApiMethod::EditRollback:
            return handle_rollback(request);
        case EApiMethod::QuestionShrink:
        {
            if (request.m_session_id.empty() || request.m_question_id == 0)
            {
                return make_error(EErrorKind::InvalidRequest,
                                  "session_id and non-zero question_id are required");
            }
            const auto session = m_conversation_store.get_session(request.m_session_id);
            if (!session.has_value())
            {
                return make_error(EErrorKind::InvalidRequest, "unknown conversation session");
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
    if (request.m_text.empty())
    {
        return make_error(EErrorKind::InvalidRequest, "empty ask text");
    }

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

    // Environment preflight. Everything the tree needs is checked and reported
    // here, with the failing stage and the path: a missing resource is a debug
    // event, not a silent "no context".
    if (!m_project_manager.has_current())
    {
        m_logger.log_event(ELogLevel::Error, "environment.preflight", {
            {"request_id", request.m_request_id},
            {"stage", "project"},
            {"fault", "project_not_open"}
        });
        return make_error(EErrorKind::ProjectNotOpen, "project is not open");
    }

    {
        const SProjectContext& context = m_project_manager.current();
        std::error_code db_error;
        const bool db_exists =
            std::filesystem::exists(context.m_project.m_db_path, db_error);
        if (!db_exists)
        {
            // No absolute paths in the journal (plugin_api.md): the log is
            // shipped in bug reports. The stage and the project id are enough
            // to locate the resource; filesystem detail goes to the message only.
            m_logger.log_event(ELogLevel::Error, "environment.preflight", {
                {"request_id", request.m_request_id},
                {"project_id", context.m_project.m_project_id},
                {"stage", "index_database"},
                {"fault", db_error ? "stat_failed" : "missing"},
                {"stat_error", db_error ? db_error.message() : std::string()}
            });
            return make_error(EErrorKind::IndexNotReady,
                              "project index database is missing: "
                              + context.m_project.m_db_path.string());
        }

        // The file exists, but it must actually open and pass the schema gate.
        CSQLiteIndex probe(context.m_project.m_db_path);
        if (!probe.open_db())
        {
            m_logger.log_event(ELogLevel::Error, "environment.preflight", {
                {"request_id", request.m_request_id},
                {"project_id", context.m_project.m_project_id},
                {"stage", "index_database"},
                {"fault", "open_failed"}
            });
            return make_error(EErrorKind::IndexNotReady,
                              "project index database cannot be opened: "
                              + context.m_project.m_db_path.string());
        }
        probe.close_db();
    }

    {
        const SIndexStatus status = m_project_manager.current().m_index_status;
        if (status.m_state != EIndexState::Ready)
        {
            m_logger.log_event(ELogLevel::Error, "environment.preflight", {
                {"request_id", request.m_request_id},
                {"stage", "index_state"},
                {"fault", "not_ready"},
                {"state", static_cast<int>(status.m_state)},
                {"files", status.m_file_count},
                {"symbols", status.m_symbol_count}
            });
            return make_error(EErrorKind::IndexNotReady,
                              "project index is not ready (state "
                              + std::to_string(static_cast<int>(status.m_state)) + ")");
        }
    }

    // Адрес диалога — забота агента, а не клиента (decisions.md №20: один
    // процесс = один проект = одна сессия на весь срок жизни процесса).
    // Активная сессия уже создана в project.open, её id отправлен клиенту, и
    // именно она и используется для ask — всегда, независимо от того, что
    // (если вообще что-то) клиент прислал в session_id. Клиент вправе вовсе
    // его не передавать.
    //
    // Присланный session_id — не параметр адресации, а необязательный флаг
    // "клиент считает, что говорит с этой же беседой". При расхождении запрос
    // не отклоняется (новый сеанс/перезапуск клиента — обычное дело), но
    // расхождение остаётся в журнале как диагностика.
    const std::string current_project_id =
        m_project_manager.current().m_project.m_project_id;
    const std::string active_session_id = [&]() -> std::string
    {
        const auto active =
            m_conversation_store.latest_session_for_project(current_project_id);
        return active.has_value() ? active->m_id : std::string();
    }();

    const std::string session_id = active_session_id;

    if (!request.m_session_id.empty() && !active_session_id.empty()
        && request.m_session_id != active_session_id)
    {
        m_logger.log_event(ELogLevel::Info, "session.client_mismatch", {
            {"request_id", request.m_request_id},
            {"project_id", current_project_id}
        });
    }

    if (session_id.empty())
    {
        // Проект открыт, но активной сессии нет: project.open обязан был её
        // создать. Пустой SID — это «агент в жопе», и это должно быть видно.
        m_logger.log_event(ELogLevel::Error, "session.missing", {
            {"request_id", request.m_request_id},
            {"project_id", current_project_id},
            {"stage", "resolve_active_session"}
        });
        return make_error(EErrorKind::IoError,
                          "no active conversation session for the open project");
    }

    std::string session_context_json = "{}";
    std::vector<SConversationTurn> conversation_history;
    std::vector<SConversationEvent> stored_events;
    {
        const auto session = m_conversation_store.get_session(session_id);
        if (!session.has_value())
        {
            return make_error(EErrorKind::InvalidRequest, "unknown conversation session");
        }
        if (!session->m_project_id.empty()
            && session->m_project_id != current_project_id)
        {
            return make_error(EErrorKind::InvalidRequest,
                              "conversation session belongs to another project");
        }
        // Сессия без проекта (созданная до open) фиксируется за первым проектом,
        // в котором её используют, — иначе она стала бы каналом смешивания
        // историй разных проектов.
        if (session->m_project_id.empty()
            && !m_conversation_store.bind_project(session_id, current_project_id))
        {
            return make_error(EErrorKind::IoError,
                              "cannot bind conversation session to the current project");
        }

        session_context_json = session->m_context_json;
        std::int64_t after_event_id = 0;
        constexpr std::size_t k_event_page_size = 1000;
        while (true)
        {
            std::vector<SConversationEvent> page = m_conversation_store.events(
                session_id, after_event_id, k_event_page_size);
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

        conversation_history = reconstruct_conversation_turns(stored_events);
        const nlohmann::json user_event = {
            {"text", request.m_text},
            {"editor_file", request.m_editor_file},
            {"editor_line", request.m_editor_line},
            {"editor_column", request.m_editor_column}
        };
        if (!m_conversation_store.append_event(session_id, "user", user_event.dump()))
        {
            return make_error(EErrorKind::IoError, "cannot persist user message");
        }
    }

    SOrchestrationRequest orchestration_request;
    orchestration_request.m_request_id = request.m_request_id;
    orchestration_request.m_text = request.m_text;
    orchestration_request.m_session_id = session_id;
    orchestration_request.m_cursor_line = request.m_editor_line;
    orchestration_request.m_cursor_column = request.m_editor_column;
    orchestration_request.m_history = conversation_history;

    // The editor may send a project-relative path; libclang needs an absolute one.
    if (!request.m_editor_file.empty())
    {
        const std::filesystem::path resolved =
            m_project_manager.resolve_path(request.m_editor_file);
        orchestration_request.m_cursor_file =
            resolved.empty() ? request.m_editor_file : resolved.string();
    }

    // libclang needs the translation unit's own flags (-I/-D/-std), otherwise a
    // file that depends on the project layout resolves to no semantic entity.
    if (!orchestration_request.m_cursor_file.empty())
    {
        orchestration_request.m_cursor_compile_arguments =
            m_project_manager.compile_arguments_for(orchestration_request.m_cursor_file);
        // The file name is enough for correlation; the absolute path stays out
        // of the journal.
        m_logger.log_event(ELogLevel::Info, "cursor.compile_arguments", {
            {"request_id", request.m_request_id},
            {"file_name", std::filesystem::path(
                orchestration_request.m_cursor_file).filename().string()},
            {"arguments", orchestration_request.m_cursor_compile_arguments.size()}
        });
    }

    CLlmLayer llm(m_config_manager.config().m_llm_roles, m_logger);
    CHistorian historian(llm);
    std::unique_ptr<IClang> clang =
        create_clang_backend(m_config_manager.config().m_clang_path);
    std::unique_ptr<CCursorContextAnalyzer> cursor_analyzer;
    if (clang)
    {
        cursor_analyzer = std::make_unique<CCursorContextAnalyzer>(*clang, llm);
    }
    else
    {
        // A missing Clang backend is a visible failure, not "no cursor context":
        // otherwise the agent silently answers without source evidence.
        m_logger.log_event(ELogLevel::Error, "cursor.backend_unavailable", {
            {"request_id", request.m_request_id},
            {"configured", !m_config_manager.config().m_clang_path.empty()}
        });
        return make_error(EErrorKind::IndexNotReady,
                          "clang backend is unavailable; cursor context cannot be resolved");
    }

    std::unique_ptr<CIndexFactProvider> facts;
    if (m_project_manager.has_current())
    {
        facts = std::make_unique<CIndexFactProvider>(
            m_project_manager.current().m_project.m_db_path,
            m_project_manager.current().m_project.m_canonical_path,
            clang.get());
    }
    CRequestOrchestrator orchestrator(m_logger, &historian,
        cursor_analyzer.get(), &llm, facts.get(),
        &m_config_manager, &m_project_manager);
    const SOrchestrationResult orchestration_result = orchestrator.run(orchestration_request);

    SApiResponse response;
    const std::string& answer = orchestration_result.m_message;
    response.m_ok = orchestration_result.m_ok;
    response.m_message = orchestration_result.m_message;

    {
        const nlohmann::json assistant_event = {
            {"text", answer},
            {"ok", response.m_ok}
        };
        if (!m_conversation_store.append_event(session_id, "assistant", assistant_event.dump()))
        {
            return make_error(EErrorKind::IoError, "answer produced but could not persist conversation event");
        }
        // The client learns the dialogue address even when it never sent one:
        // the id comes back on every answer.
        response.m_session_id = session_id;
    }
    if (response.m_ok)
    {
        response.m_state = "COMPLETED";
        response.m_text = answer;
    }
    else
    {
        // Дерево уже решило, почему запрос не прошёл (m_exit_reason) —
        // граница API читает это решение, а не угадывает заново по тексту.
        response.m_error = error_kind_for_exit_reason(orchestration_result.m_exit_reason);
        response.m_message = answer;
    }
    return response;
}

SApiResponse CAgentApi::handle_rollback(const SApiRequest& request)
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
        return make_error(EErrorKind::PermissionDenied,
                          "rollback writes to the project tree; allow_write is NO");
    }
    if (request.m_rollback_ref.empty() && request.m_rollback_steps == 0)
    {
        return make_error(EErrorKind::InvalidRequest,
                          "either rollback_ref or rollback_steps (>0) is required");
    }

    const std::filesystem::path project_root =
        m_project_manager.current().m_project.m_canonical_path;
    const std::string project_id = m_project_manager.current().m_project.m_project_id;
    const CGitSnapshotStore snapshots(m_config_manager.config().m_snapshot_root);

    std::string target = request.m_rollback_ref;
    std::string error;
    if (target.empty())
    {
        const std::optional<std::string> resolved =
            snapshots.resolve_steps_back(project_root, project_id, request.m_rollback_steps, error);
        if (!resolved.has_value())
        {
            return make_error(EErrorKind::SnapshotNotFound, error);
        }
        target = *resolved;
    }

    if (!snapshots.restore(project_root, project_id, target, error))
    {
        return make_error(EErrorKind::SnapshotNotFound, error);
    }

    SApiResponse response;
    response.m_ok = true;
    response.m_text = target;
    response.m_message = "project restored to snapshot " + target;
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

} // namespace fcpp
