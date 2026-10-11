/**
 * @file CPatchValidator.cpp
 * @brief Реализация валидации и планирования патча (§46).
 */

#include "core/CPatchValidator.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fcpp
{

namespace
{

/**
 * @brief Прочитать файл целиком.
 * @param path Путь.
 * @param out_content Выходное содержимое.
 * @return true при успехе.
 */
bool read_file(const std::filesystem::path& path, std::string& out_content)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    out_content = buffer.str();
    return true;
}

/**
 * @brief Разбить содержимое на строки (без сохранения терминаторов).
 * @param content Содержимое.
 * @return Строки.
 */
std::vector<std::string> split_lines(const std::string& content)
{
    std::vector<std::string> lines;
    std::istringstream input(content);
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

} // namespace

CPatchValidator::CPatchValidator(std::filesystem::path project_root)
    : m_project_root(std::move(project_root))
{
}

bool CPatchValidator::apply_hunks(const std::string& content,
                                  const SFilePatch& file_patch,
                                  std::string& out_updated,
                                  std::string& out_message)
{
    std::vector<std::string> lines = split_lines(content);
    std::vector<std::string> result;

    std::size_t cursor = 0; // 0-базовый индекс в lines

    for (const SPatchHunk& hunk : file_patch.m_hunks)
    {
        const std::size_t hunk_start = (hunk.m_old_start == 0) ? 0 : (hunk.m_old_start - 1);
        if (hunk_start < cursor || hunk_start > lines.size())
        {
            out_message = "hunk offset out of range";
            return false;
        }

        // Контекст до hunk'а.
        for (std::size_t index = cursor; index < hunk_start; ++index)
        {
            result.push_back(lines[index]);
        }

        std::size_t position = hunk_start;
        for (const SPatchLine& line : hunk.m_lines)
        {
            if (line.m_type == ' ')
            {
                if (position >= lines.size() || lines[position] != line.m_text)
                {
                    out_message = "context mismatch";
                    return false;
                }
                result.push_back(lines[position]);
                ++position;
            }
            else if (line.m_type == '-')
            {
                if (position >= lines.size() || lines[position] != line.m_text)
                {
                    out_message = "removed line mismatch";
                    return false;
                }
                ++position;
            }
            else if (line.m_type == '+')
            {
                result.push_back(line.m_text);
            }
        }

        cursor = position;
    }

    for (std::size_t index = cursor; index < lines.size(); ++index)
    {
        result.push_back(lines[index]);
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        output << result[index];
        if (index + 1 < result.size())
        {
            output << '\n';
        }
    }
    if (!result.empty())
    {
        output << '\n';
    }

    out_updated = output.str();
    return true;
}

bool CPatchValidator::validate(const SPatchSet& patch, SPatchValidation& out_validation) const
{
    out_validation = SPatchValidation{};

    if (patch.m_files.empty())
    {
        out_validation.m_message = "empty patch";
        return false;
    }

    std::error_code path_error;
    const std::filesystem::path canonical_root =
        std::filesystem::weakly_canonical(m_project_root, path_error);
    if (path_error)
    {
        out_validation.m_message = "cannot resolve project root: " + path_error.message();
        return false;
    }

    for (const SFilePatch& file_patch : patch.m_files)
    {
        const std::string relative = file_patch.m_is_create ? file_patch.m_new_path : file_patch.m_old_path;
        if (relative.empty())
        {
            out_validation.m_message = "patch entry without path";
            return false;
        }

        std::filesystem::path resolved(relative);
        if (resolved.is_relative())
        {
            resolved = canonical_root / resolved;
        }

        // Resolve existing path components (including symlinks) before checking
        // containment. A string-prefix check incorrectly accepts sibling paths
        // such as "/project-evil" when the root is "/project".
        path_error.clear();
        resolved = std::filesystem::weakly_canonical(resolved, path_error);
        if (path_error)
        {
            out_validation.m_message = "cannot resolve patch path: " + relative
                + ": " + path_error.message();
            return false;
        }

        const std::filesystem::path relative_to_root = resolved.lexically_relative(canonical_root);
        if (relative_to_root.empty() || relative_to_root == "."
                || relative_to_root.is_absolute() || *relative_to_root.begin() == "..")
        {
            out_validation.m_message = "path escapes project root: " + relative;
            return false;
        }

        SFileChange change;
        change.m_path = resolved.string();
        change.m_is_create = file_patch.m_is_create;
        change.m_is_delete = file_patch.m_is_delete;

        std::error_code error_code;
        change.m_existed = std::filesystem::exists(resolved, error_code) && !error_code;

        if (change.m_is_create && change.m_existed)
        {
            out_validation.m_message = "create target already exists: " + relative;
            return false;
        }
        if (!change.m_is_create && !change.m_existed)
        {
            out_validation.m_message = "file not found: " + relative;
            return false;
        }

        std::string original;
        if (change.m_existed && !read_file(resolved, original))
        {
            out_validation.m_message = "cannot read: " + relative;
            return false;
        }
        change.m_original = original;

        if (change.m_is_delete)
        {
            if (file_patch.m_hunks.empty() && !original.empty())
            {
                out_validation.m_message = "delete patch has no hunks: " + relative;
                return false;
            }

            std::string verified_content;
            std::string delete_message;
            if (!apply_hunks(original, file_patch, verified_content, delete_message))
            {
                out_validation.m_message = "delete patch conflict: " + delete_message;
                return false;
            }
            if (!verified_content.empty())
            {
                out_validation.m_message = "delete patch does not remove entire file: " + relative;
                return false;
            }
            change.m_updated.clear();
        }
        else if (change.m_is_create)
        {
            std::string updated;
            std::ostringstream builder;
            for (const SPatchHunk& hunk : file_patch.m_hunks)
            {
                for (const SPatchLine& line : hunk.m_lines)
                {
                    if (line.m_type == '+')
                    {
                        builder << line.m_text << '\n';
                    }
                }
            }
            change.m_updated = builder.str();
        }
        else
        {
            std::string updated;
            std::string message;
            if (!apply_hunks(original, file_patch, updated, message))
            {
                out_validation.m_message = relative + ": " + message;
                return false;
            }
            change.m_updated = updated;
        }

        out_validation.m_changes.push_back(std::move(change));
    }

    out_validation.m_valid = true;
    return true;
}

} // namespace fcpp
