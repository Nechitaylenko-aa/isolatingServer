/**
 * @file CGitProcess.cpp
 * @brief Реализация CGitProcess::run.
 */

#include "core/CGitProcess.h"
#include "core/CProcessRunner.h"

namespace fcpp
{

SExecutionResult CGitProcess::run(const std::filesystem::path& git_dir,
                                  const std::filesystem::path& work_tree,
                                  const std::vector<std::string>& args,
                                  std::uint32_t timeout_seconds) const
{
    SCommandArgs command;
    command.m_tool = "git";
    command.m_working_dir = work_tree;
    command.m_timeout_seconds = timeout_seconds;

    // Идентичность коммитера задаётся явно (-c), а не через глобальный git
    // config пользователя — теневой репозиторий не должен зависеть от того,
    // настроен ли у пользователя `git config user.name` вообще.
    command.m_arguments = {
        "-c", "user.name=fcpp-agent",
        "-c", "user.email=fcpp-agent@localhost",
        "--git-dir=" + git_dir.string(),
        "--work-tree=" + work_tree.string()
    };
    command.m_arguments.insert(command.m_arguments.end(), args.begin(), args.end());

    return spawn_process(command);
}

} // namespace fcpp
