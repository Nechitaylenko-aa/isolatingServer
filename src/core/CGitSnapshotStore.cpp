/**
 * @file CGitSnapshotStore.cpp
 * @brief Реализация CGitSnapshotStore — см. заголовок.
 */

#include "core/CGitSnapshotStore.h"

#include <fstream>

namespace fcpp
{

namespace
{

/** @brief Убрать один завершающий перевод строки, если он есть. */
std::string trim_trailing_newline(std::string text)
{
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
    {
        text.pop_back();
    }
    return text;
}

/** @brief Разбить строку по байту-разделителю (ASCII Unit Separator). */
std::vector<std::string> split_unit_separator(const std::string& line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t pos = line.find('\x1f', start);
        if (pos == std::string::npos)
        {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return fields;
}

} // namespace

CGitSnapshotStore::CGitSnapshotStore(std::filesystem::path snapshot_root)
    : m_snapshot_root(std::move(snapshot_root))
{
}

std::filesystem::path CGitSnapshotStore::git_dir(const std::string& project_id) const
{
    return m_snapshot_root / project_id / "snapshots.git";
}

bool CGitSnapshotStore::ensure_initialized(const std::filesystem::path& project_root,
                                           const std::string& project_id,
                                           std::string& error) const
{
    const std::filesystem::path dir = git_dir(project_id);
    std::error_code fs_error;

    if (std::filesystem::exists(dir / "HEAD", fs_error))
    {
        return true;
    }

    std::filesystem::create_directories(dir.parent_path(), fs_error);
    if (fs_error)
    {
        error = "cannot create snapshot directory: " + fs_error.message();
        return false;
    }

    // Намеренно без --bare: --git-dir и --work-tree заданы одновременно (в
    // CGitProcess::run), git по этому сочетанию сам настраивает
    // core.bare=false/core.worktree — ровно та развязка git-dir/work-tree,
    // которая нужна (а не классический bare-репозиторий без рабочего
    // дерева вообще). Не проверено на реальном git — первое, что стоит
    // перепроверить при сборке.
    const SExecutionResult init_result = m_git.run(dir, project_root, {"init"});
    if (!init_result.m_allowed || init_result.m_exit_code != 0)
    {
        error = "git init failed: " + init_result.m_stderr;
        return false;
    }

    // Защита от рекурсии в собственный .git проекта (если он у проекта есть)
    // — независимо от того, что уже исключено через .gitignore проекта.
    std::ofstream exclude(dir / "info" / "exclude", std::ios::app);
    if (exclude.is_open())
    {
        exclude << ".git\n.git/\n";
    }

    return true;
}

bool CGitSnapshotStore::snapshot(const std::filesystem::path& project_root,
                                 const std::string& project_id,
                                 const std::string& message,
                                 std::string& error) const
{
    if (!ensure_initialized(project_root, project_id, error))
    {
        return false;
    }

    const std::filesystem::path dir = git_dir(project_id);

    const SExecutionResult add_result = m_git.run(dir, project_root, {"add", "-A"});
    if (!add_result.m_allowed || add_result.m_exit_code != 0)
    {
        error = "git add failed: " + add_result.m_stderr;
        return false;
    }

    const SExecutionResult commit_result = m_git.run(
        dir, project_root, {"commit", "--allow-empty", "-m", message});
    if (!commit_result.m_allowed || commit_result.m_exit_code != 0)
    {
        error = "git commit failed: " + commit_result.m_stderr;
        return false;
    }

    return true;
}

bool CGitSnapshotStore::restore(const std::filesystem::path& project_root,
                                const std::string& project_id,
                                const std::string& ref,
                                std::string& error) const
{
    const std::filesystem::path dir = git_dir(project_id);
    std::error_code fs_error;
    if (!std::filesystem::exists(dir / "HEAD", fs_error))
    {
        error = "no snapshot history for this project yet";
        return false;
    }

    // "-- ." — путь-спецификатор: обновляет только рабочее дерево/индекс
    // из ref, не трогает HEAD/branch. Не классический checkout, detached
    // HEAD не возникает (plans/git-snapshot-rollback.md, пункт 6 — снят).
    const SExecutionResult result = m_git.run(dir, project_root, {"checkout", ref, "--", "."});
    if (!result.m_allowed || result.m_exit_code != 0)
    {
        error = "git checkout failed: " + result.m_stderr;
        return false;
    }

    return true;
}

std::optional<std::string> CGitSnapshotStore::resolve_steps_back(
    const std::filesystem::path& project_root,
    const std::string& project_id,
    unsigned steps,
    std::string& error) const
{
    const std::filesystem::path dir = git_dir(project_id);
    std::error_code fs_error;
    if (!std::filesystem::exists(dir / "HEAD", fs_error))
    {
        error = "no snapshot history for this project yet";
        return std::nullopt;
    }

    const SExecutionResult result = m_git.run(
        dir, project_root, {"rev-parse", "HEAD~" + std::to_string(steps)});
    if (!result.m_allowed || result.m_exit_code != 0)
    {
        error = "fewer than " + std::to_string(steps) + " snapshots in history";
        return std::nullopt;
    }

    return trim_trailing_newline(result.m_stdout);
}

std::vector<SSnapshotEntry> CGitSnapshotStore::list(const std::filesystem::path& project_root,
                                                    const std::string& project_id,
                                                    unsigned limit,
                                                    std::string& error) const
{
    const std::filesystem::path dir = git_dir(project_id);
    std::error_code fs_error;
    if (!std::filesystem::exists(dir / "HEAD", fs_error))
    {
        // Ещё ни одного снапшота — пустая история, не ошибка.
        return {};
    }

    const SExecutionResult result = m_git.run(dir, project_root,
        {"log", "--format=%H\x1f%cI\x1f%s", "-n", std::to_string(limit)});

    if (!result.m_allowed)
    {
        error = "git log failed to start: " + result.m_stderr;
        return {};
    }
    if (result.m_exit_code != 0)
    {
        // Самая частая причина — ноль коммитов в свежей истории: тоже
        // пустой список, не ошибка.
        return {};
    }

    std::vector<SSnapshotEntry> entries;
    std::size_t start = 0;
    while (start < result.m_stdout.size())
    {
        std::size_t end = result.m_stdout.find('\n', start);
        if (end == std::string::npos)
        {
            end = result.m_stdout.size();
        }
        const std::string line = result.m_stdout.substr(start, end - start);
        start = end + 1;

        if (line.empty())
        {
            continue;
        }

        const std::vector<std::string> fields = split_unit_separator(line);
        SSnapshotEntry entry;
        entry.m_sha = fields.size() > 0 ? fields[0] : std::string();
        entry.m_timestamp = fields.size() > 1 ? fields[1] : std::string();
        entry.m_message = fields.size() > 2 ? fields[2] : std::string();
        entries.push_back(std::move(entry));
    }

    return entries;
}

} // namespace fcpp
