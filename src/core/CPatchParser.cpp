/**
 * @file CPatchParser.cpp
 * @brief Реализация парсера unified diff (§44).
 */

#include "core/CPatchParser.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace fcpp
{

namespace
{

/**
 * @brief Убрать префикс a/ или b/ из пути.
 * @param path Путь из заголовка патча.
 * @return Путь без префикса.
 */
std::string strip_prefix(const std::string& path)
{
    if (path.rfind("a/", 0) == 0 || path.rfind("b/", 0) == 0)
    {
        return path.substr(2);
    }
    return path;
}

/**
 * @brief Разобрать заголовок hunk'а @@ -old,c +new,c @@.
 * @param line Строка заголовка.
 * @param hunk Выходной hunk.
 * @return true при успехе.
 */
bool parse_hunk_header(const std::string& line, SPatchHunk& hunk)
{
    const std::size_t first = line.find('-');
    const std::size_t plus = line.find('+');
    if (first == std::string::npos || plus == std::string::npos)
    {
        return false;
    }

    std::string old_part = line.substr(first + 1, plus - first - 1);
    const std::size_t second_at = line.find('@', plus);
    std::string new_part = line.substr(plus + 1, (second_at == std::string::npos) ? std::string::npos : second_at - plus - 1);

    auto parse_range = [](const std::string& part, std::uint32_t& start, std::uint32_t& count)
    {
        const std::size_t comma = part.find(',');
        const std::string start_text = (comma == std::string::npos) ? part : part.substr(0, comma);
        const std::string count_text = (comma == std::string::npos) ? "1" : part.substr(comma + 1);
        try
        {
            start = static_cast<std::uint32_t>(std::stoul(start_text));
            count = static_cast<std::uint32_t>(std::stoul(count_text));
        }
        catch (const std::exception&)
        {
            return false;
        }
        return true;
    };

    return parse_range(old_part, hunk.m_old_start, hunk.m_old_count)
           && parse_range(new_part, hunk.m_new_start, hunk.m_new_count);
}

} // namespace

bool CPatchParser::parse(const std::string& text, SPatchSet& out_set)
{
    out_set = SPatchSet{};

    std::istringstream input(text);
    std::string line;

    SFilePatch current_file;
    bool in_file = false;
    SPatchHunk current_hunk;
    bool in_hunk = false;
    std::uint32_t old_line = 0;
    std::uint32_t new_line = 0;

    auto flush_hunk = [&]()
    {
        if (in_hunk)
        {
            current_file.m_hunks.push_back(current_hunk);
            current_hunk = SPatchHunk{};
            in_hunk = false;
        }
    };

    auto flush_file = [&]()
    {
        flush_hunk();
        if (in_file)
        {
            out_set.m_files.push_back(current_file);
            current_file = SFilePatch{};
            in_file = false;
        }
    };

    while (std::getline(input, line))
    {
        if (line.rfind("diff --git", 0) == 0)
        {
            flush_file();
            in_file = true;
            continue;
        }

        if (line.rfind("--- ", 0) == 0)
        {
            flush_hunk();
            in_file = true;
            const std::string path = line.substr(4);
            if (path == "/dev/null")
            {
                current_file.m_is_create = true;
                current_file.m_old_path.clear();
            }
            else
            {
                current_file.m_old_path = strip_prefix(path);
            }
            continue;
        }

        if (line.rfind("+++ ", 0) == 0)
        {
            flush_hunk();
            const std::string path = line.substr(4);
            if (path == "/dev/null")
            {
                current_file.m_is_delete = true;
                current_file.m_new_path.clear();
            }
            else
            {
                current_file.m_new_path = strip_prefix(path);
            }
            continue;
        }

        if (line.rfind("@@", 0) == 0)
        {
            flush_hunk();

            SPatchHunk hunk;
            if (!parse_hunk_header(line, hunk))
            {
                return false;
            }
            current_hunk = hunk;
            in_hunk = true;
            old_line = current_hunk.m_old_start;
            new_line = current_hunk.m_new_start;
            continue;
        }

        if (!in_hunk)
        {
            // Строки вне hunk'а (заголовки index/--- и т.п.) игнорируются.
            continue;
        }

        if (line.empty())
        {
            // Пустая строка в hunk'е трактуется как пустая контекстная строка.
            SPatchLine patch_line;
            patch_line.m_type = ' ';
            patch_line.m_line = old_line;
            current_hunk.m_lines.push_back(patch_line);
            ++old_line;
            ++new_line;
            continue;
        }

        const char marker = line.front();
        const std::string body = line.substr(1);

        SPatchLine patch_line;
        patch_line.m_text = body;

        if (marker == ' ')
        {
            patch_line.m_type = ' ';
            patch_line.m_line = old_line;
            ++old_line;
            ++new_line;
        }
        else if (marker == '-')
        {
            patch_line.m_type = '-';
            patch_line.m_line = old_line;
            ++old_line;
        }
        else if (marker == '+')
        {
            patch_line.m_type = '+';
            patch_line.m_line = new_line;
            ++new_line;
        }
        else if (marker == '\\')
        {
            // "\ No newline at end of file" — игнорируется.
            continue;
        }
        else
        {
            return false;
        }

        current_hunk.m_lines.push_back(std::move(patch_line));
    }

    flush_file();
    return !out_set.m_files.empty();
}

} // namespace fcpp
