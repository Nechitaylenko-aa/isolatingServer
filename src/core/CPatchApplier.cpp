/**
 * @file CPatchApplier.cpp
 * @brief Реализация атомарного применения патча (§77).
 */

#include "core/CPatchApplier.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace fcpp
{

namespace
{

/**
 * @brief Записать содержимое в файл.
 * @param path Путь.
 * @param content Содержимое.
 * @return true при успехе.
 */
bool write_file(const std::filesystem::path& path, const std::string& content)
{
    std::error_code error_code;
    std::filesystem::create_directories(path.parent_path(), error_code);
    if (error_code)
    {
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return false;
    }
    output << content;
    output.close();
    return output.good();
}

bool remove_path_if_present(const std::string& path)
{
    std::error_code error_code;
    const bool removed = std::filesystem::remove(path, error_code);
    if (error_code || removed)
    {
        return !error_code;
    }

    const bool remains = std::filesystem::exists(path, error_code);
    return !error_code && !remains;
}

} // namespace

bool CPatchApplier::revert(const std::vector<SFileChange>& changes, std::string& out_message) const
{
    for (const SFileChange& change : changes)
    {
        if (change.m_is_delete)
        {
            // Удалённый файл восстанавливаем из оригинала.
            if (!write_file(change.m_path, change.m_original))
            {
                out_message = "revert failed: " + change.m_path;
                return false;
            }
        }
        else if (change.m_existed)
        {
            if (!write_file(change.m_path, change.m_original))
            {
                out_message = "revert failed: " + change.m_path;
                return false;
            }
        }
        else
        {
            if (!remove_path_if_present(change.m_path))
            {
                out_message = "revert failed: " + change.m_path;
                return false;
            }
        }
    }
    return true;
}

bool CPatchApplier::apply(const std::vector<SFileChange>& changes, std::string& out_message) const
{
    std::vector<std::size_t> applied;

    for (std::size_t index = 0; index < changes.size(); ++index)
    {
        const SFileChange& change = changes[index];

        bool ok = false;
        if (change.m_is_delete)
        {
            std::error_code error_code;
            ok = std::filesystem::remove(change.m_path, error_code);
            if (error_code)
            {
                ok = false;
            }
        }
        else
        {
            ok = write_file(change.m_path, change.m_updated);
        }

        if (!ok)
        {
            out_message = "apply failed: " + change.m_path;

            // Откат ранее применённых изменений.
            for (std::size_t rollback = applied.size(); rollback > 0; --rollback)
            {
                const SFileChange& done = changes[applied[rollback - 1]];
                bool rollback_ok = false;
                if (done.m_is_delete || done.m_existed)
                {
                    rollback_ok = write_file(done.m_path, done.m_original);
                }
                else
                {
                    rollback_ok = remove_path_if_present(done.m_path);
                }
                if (!rollback_ok)
                {
                    out_message += "; rollback incomplete: " + done.m_path;
                }
            }
            return false;
        }

        applied.push_back(index);
    }

    return true;
}

} // namespace fcpp
