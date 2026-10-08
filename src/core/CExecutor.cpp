/**
 * @file CExecutor.cpp
 * @brief Реализация безопасного исполнителя внешних утилит (ADR-08).
 */

#include "core/CExecutor.h"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

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

/** @brief Размер буфера чтения из пайпа. */
constexpr std::size_t k_read_buffer_size = 4096;

/** @brief Код выхода дочернего процесса при ошибке exec. */
constexpr int k_exec_failure_code = 127;

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
    SExecutionResult result;
    result.m_allowed = true;
    result.m_exit_code = -1;

    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (pipe(stdout_pipe) != 0)
    {
        result.m_allowed = false;
        result.m_stderr = "pipe creation failed";
        return result;
    }

    if (pipe(stderr_pipe) != 0)
    {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        result.m_allowed = false;
        result.m_stderr = "pipe creation failed";
        return result;
    }

    const pid_t pid = fork();
    if (pid < 0)
    {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        result.m_allowed = false;
        result.m_stderr = "fork failed";
        return result;
    }

    if (pid == 0)
    {
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0)
        {
            _exit(k_exec_failure_code);
        }
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0)
        {
            _exit(k_exec_failure_code);
        }

        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        if (!args.m_working_dir.empty())
        {
            if (chdir(args.m_working_dir.c_str()) != 0)
            {
                _exit(k_exec_failure_code);
            }
        }

        std::vector<std::string> tokens;
        tokens.reserve(args.m_arguments.size() + 1);
        tokens.push_back(args.m_tool);
        for (const std::string& argument : args.m_arguments)
        {
            tokens.push_back(argument);
        }

        std::vector<char*> argv;
        argv.reserve(tokens.size() + 1);
        for (std::string& token : tokens)
        {
            argv.push_back(token.data());
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        _exit(k_exec_failure_code);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::seconds(args.m_timeout_seconds);

    bool stdout_open = true;
    bool stderr_open = true;
    bool timed_out = false;

    char buffer[k_read_buffer_size];

    while (stdout_open || stderr_open)
    {
        struct pollfd descriptors[2];
        nfds_t descriptor_count = 0;
        int stdout_slot = -1;
        int stderr_slot = -1;

        if (stdout_open)
        {
            descriptors[descriptor_count].fd = stdout_pipe[0];
            descriptors[descriptor_count].events = POLLIN;
            descriptors[descriptor_count].revents = 0;
            stdout_slot = static_cast<int>(descriptor_count);
            ++descriptor_count;
        }
        if (stderr_open)
        {
            descriptors[descriptor_count].fd = stderr_pipe[0];
            descriptors[descriptor_count].events = POLLIN;
            descriptors[descriptor_count].revents = 0;
            stderr_slot = static_cast<int>(descriptor_count);
            ++descriptor_count;
        }

        int poll_timeout = -1;
        if (args.m_timeout_seconds > 0)
        {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
            {
                timed_out = true;
                break;
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            poll_timeout = static_cast<int>(remaining);
            if (poll_timeout < 0)
            {
                poll_timeout = 0;
            }
        }

        const int poll_result = poll(descriptors, descriptor_count, poll_timeout);
        if (poll_result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (poll_result == 0)
        {
            timed_out = true;
            break;
        }

        for (nfds_t slot = 0; slot < descriptor_count; ++slot)
        {
            if ((descriptors[slot].revents & (POLLIN | POLLHUP)) == 0)
            {
                continue;
            }

            const ssize_t read_count = read(descriptors[slot].fd, buffer, sizeof(buffer));
            if (read_count > 0)
            {
                if (static_cast<int>(slot) == stdout_slot)
                {
                    result.m_stdout.append(buffer, static_cast<std::size_t>(read_count));
                }
                else if (static_cast<int>(slot) == stderr_slot)
                {
                    result.m_stderr.append(buffer, static_cast<std::size_t>(read_count));
                }
            }
            else
            {
                if (static_cast<int>(slot) == stdout_slot)
                {
                    stdout_open = false;
                }
                else if (static_cast<int>(slot) == stderr_slot)
                {
                    stderr_open = false;
                }
            }
        }
    }

    if (timed_out)
    {
        kill(pid, SIGKILL);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        result.m_exit_code = -1;
    }
    else if (WIFEXITED(status))
    {
        result.m_exit_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        result.m_exit_code = 128 + WTERMSIG(status);
    }

    if (timed_out)
    {
        if (!result.m_stderr.empty())
        {
            result.m_stderr.append("\n");
        }
        result.m_stderr.append("command timed out");
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    return result;
}

} // namespace fcpp
