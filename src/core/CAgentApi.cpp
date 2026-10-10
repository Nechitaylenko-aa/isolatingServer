/**
 * @file CAgentApi.cpp
 * @brief Реализация Agent API (§89, §93).
 */

#include "core/CAgentApi.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "core/CAskService.h"
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
    "session.update_context"
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
{
    m_conversation_store.open(m_config_manager.agent_dir() / "sessions.sqlite");
}

SApiResponse CAgentApi::execute(const SApiRequest& request)
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
            if (target.empty() || !m_project_manager.open(target))
            {
                return make_error(EErrorKind::InvalidRequest, "cannot open project: " + target);
            }
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
            if (!m_project_manager.has_current())
            {
                m_project_manager.restore_session();
            }
            if (!m_project_manager.refresh_index())
            {
                return make_error(EErrorKind::IndexNotReady, "index refresh failed");
            }
            response.m_ok = true;
            response.m_index_status = m_project_manager.current().m_index_status;
            return response;
        }
        case EApiMethod::IndexRebuild:
        {
            if (!m_project_manager.has_current())
            {
                m_project_manager.restore_session();
            }
            if (!m_project_manager.rebuild_index())
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
            if (!m_project_manager.has_current())
            {
                m_project_manager.restore_session();
            }
            if (!m_project_manager.has_current())
            {
                return make_error(EErrorKind::ProjectNotOpen, "project not open");
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
            if (!m_conversation_store.create_session(session_id, project_id))
            {
                return make_error(EErrorKind::IoError, "cannot create conversation session");
            }
            nlohmann::json payload = {{"project_id", project_id}};
            if (!m_conversation_store.append_event(session_id, "session.open", payload.dump()))
            {
                m_conversation_store.close_session(session_id);
                return make_error(EErrorKind::IoError, "cannot persist session.open event");
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
            if (!m_conversation_store.append_event(request.m_session_id, "session.close", "{}"))
            {
                return make_error(EErrorKind::InvalidRequest, "unknown or closed session");
            }
            if (!m_conversation_store.close_session(request.m_session_id))
            {
                return make_error(EErrorKind::IoError, "cannot close conversation session");
            }
            response.m_ok = true;
            response.m_session_id = request.m_session_id;
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
            if (!m_conversation_store.update_context(request.m_session_id, request.m_context_json))
            {
                return make_error(EErrorKind::InvalidRequest, "unknown or closed session");
            }
            if (!m_conversation_store.append_event(request.m_session_id,
                                                   "session.context_updated",
                                                   request.m_context_json))
            {
                return make_error(EErrorKind::IoError, "cannot persist context update event");
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

    if (!m_project_manager.has_current())
    {
        m_project_manager.restore_session();
    }

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
        nlohmann::json user_event = {
            {"text", request.m_text},
            {"editor_file", request.m_editor_file},
            {"editor_line", request.m_editor_line},
            {"editor_column", request.m_editor_column}
        };
        if (!m_conversation_store.append_event(request.m_session_id, "user", user_event.dump()))
        {
            return make_error(EErrorKind::IoError, "cannot persist user message");
        }
    }

    // Тело запроса ask — text + editor (§94); всё прочее выводит ядро.
    CAskService service(m_config_manager, m_project_manager);
    SApiResponse response;
    std::string answer;
    response.m_ok = service.ask(request.m_text,
                                request.m_stream,
                                request.m_editor_file,
                                request.m_editor_line,
                                request.m_editor_column,
                                answer);
    if (!request.m_session_id.empty())
    {
        nlohmann::json assistant_event = {
            {"text", answer},
            {"ok", response.m_ok}
        };
        if (!m_conversation_store.append_event(request.m_session_id, "assistant", assistant_event.dump()))
        {
            return make_error(EErrorKind::IoError, "answer produced but could not persist conversation event");
        }
        response.m_session_id = request.m_session_id;
    }
    if (response.m_ok)
    {
        response.m_state = "COMPLETED";
        response.m_text = answer;
    }
    else if (!answer.empty())
    {
        // Уточняющий вопрос (§22): ответ несёт вопрос со списком вариантов.
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
