/**
 * @file CAgentApi.cpp
 * @brief Реализация Agent API (§89, §93).
 */

#include "core/CAgentApi.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

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
    "doctor"
};

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
{
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
            if (request.m_path.empty() || !m_project_manager.open(request.m_path))
            {
                return make_error(EErrorKind::InvalidRequest, "cannot open project: " + request.m_path);
            }
            m_project_manager.save_session();
            response.m_ok = true;
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
            if (!m_project_manager.has_current() && !m_project_manager.restore_session())
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
            if (!m_project_manager.has_current())
            {
                m_project_manager.restore_session();
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
        default:
            return make_error(EErrorKind::InvalidRequest, "unknown method");
    }
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

    SApiResponse response;
    response.m_ok = true;
    response.m_text = "patch applied (" + std::to_string(validation.m_changes.size()) + " files)";
    return response;
}

} // namespace fcpp
