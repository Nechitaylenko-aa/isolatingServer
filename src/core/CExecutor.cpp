/**
 * @file CExecutor.cpp
 * @brief Реализация безопасного исполнителя внешних утилит (ADR-08).
 */

#include "core/CExecutor.h"
#include "core/CProcessRunner.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace fcpp
{

namespace
{

/**
 * @brief Индекс разрешённых утилит (whitelist).
 */
enum class EAllowedTools
{
    CMake,     ///< cmake.
    Ninja,     ///< ninja.
    CTest,     ///< ctest.
    ClangTidy, ///< clang-tidy.
    Count      ///< Маркер количества для инициализации массива.
};

/**
 * @brief Имена разрешённых утилит, индексированные по EAllowedTools.
 */
static const char* const m_allowed_tool_names[static_cast<std::size_t>(EAllowedTools::Count)]
{
    "cmake",
    "ninja",
    "ctest",
    "clang-tidy"
};

/** @brief Запрещённые shell-метасимволы (закрытая модель безопасности). */
static const char* const m_forbidden_characters = ";|&$><()\n";

} // namespace

bool CExecutor::is_tool_allowed(const std::string& tool)
{
    const std::string base = std::filesystem::path(tool).filename().string();
    for (std::size_t index = 0; index < static_cast<std::size_t>(EAllowedTools::Count); ++index)
    {
        if (base == m_allowed_tool_names[index])
        {
            return true;
        }
    }
    return false;
}

bool CExecutor::has_forbidden_metacharacters(const std::string& text)
{
    return text.find_first_of(m_forbidden_characters) != std::string::npos;
}

EDecision CExecutor::check(const SCommandArgs& args) const
{
    if (args.m_tool.empty())
    {
        return DECISION_DENIED;
    }

    if (has_forbidden_metacharacters(args.m_tool))
    {
        return DECISION_DENIED;
    }

    for (const std::string& argument : args.m_arguments)
    {
        if (has_forbidden_metacharacters(argument))
        {
            return DECISION_DENIED;
        }
    }

    if (!is_tool_allowed(args.m_tool))
    {
        return DECISION_DENIED;
    }

    if (args.m_effect == EFFECT_WRITE_SOURCE)
    {
        return DECISION_CONFIRMATION_REQUIRED;
    }

    return DECISION_ALLOWED;
}

SExecutionResult CExecutor::execute(const SCommandArgs& args,
                                    const std::function<bool(const SCommandArgs&)>& confirm)
{
    SExecutionResult result;
    result.m_allowed = false;
    result.m_exit_code = -1;

    const EDecision decision = check(args);
    if (decision == DECISION_DENIED)
    {
        result.m_stderr = "command denied by policy";
        return result;
    }

    if (decision == DECISION_CONFIRMATION_REQUIRED)
    {
        if (!confirm || !confirm(args))
        {
            result.m_stderr = "command not confirmed";
            return result;
        }
    }

    return run_process(args);
}

SExecutionResult CExecutor::run_process(const SCommandArgs& args)
{
    // Механика запуска вынесена в CProcessRunner (общая с CGitProcess —
    // внутренним механизмом теневого git-снапшота, который сознательно НЕ
    // проходит через whitelist этого класса, см. CProcessRunner.h).
    // check()/execute() выше — единственное место, где whitelist применяется
    // к тому, что реально мутирует пользовательский проект.
    return spawn_process(args);
}

} // namespace fcpp
