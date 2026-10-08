/**
 * @file CProjectManager.cpp
 * @brief Реализация управления проектами (ADR-03).
 */

#include "core/CProjectManager.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#include <memory>

#include "core/CHash.h"
#include "core/CCompileCommandsLocator.h"
#include "core/CCompileCommandsParser.h"
#include "index/CClangFactory.h"
#include "index/CSQLiteIndex.h"
#include "index/CSchemaMigrator.h"

namespace fcpp
{

namespace
{

/** @brief Префикс непрозрачного идентификатора проекта. */
constexpr const char* k_project_id_prefix = "project-";

/** @brief Длина hex-префикса хэша пути для project_id. */
constexpr std::size_t k_project_id_hash_length = 8;

/** @brief Длина hex-префикса хэша пути для каталога базы индекса. */
constexpr std::size_t k_db_hash_length = 16;

/** @brief Имя файла базы индекса. */
constexpr const char* k_index_db_name = "index.db";

/** @brief Имя файла CMake-корня, обязательного для проекта. */
constexpr const char* k_cmake_lists_name = "CMakeLists.txt";

/**
 * @brief Текущее время в unix-секундах.
 * @return Число секунд с epoch.
 */
std::int64_t now_unix_seconds()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

} // namespace

CProjectManager::CProjectManager(const CConfigManager& config_manager)
    : m_config_manager(config_manager)
    , m_registry(config_manager.config().m_registry_path)
    , m_compile_commands_path()
    , m_has_current(false)
    , m_current()
{
}

CProjectManager::~CProjectManager() = default;

void CProjectManager::set_compile_commands_path(const std::filesystem::path& path)
{
    m_compile_commands_path = path;
}

bool CProjectManager::open(const std::filesystem::path& path)
{
    std::error_code error_code;
    const std::filesystem::path canonical = std::filesystem::canonical(path, error_code);
    if (error_code)
    {
        return false;
    }

    if (!std::filesystem::is_directory(canonical, error_code) || error_code)
    {
        return false;
    }

    const std::filesystem::path cmake_lists = canonical / k_cmake_lists_name;
    if (!std::filesystem::exists(cmake_lists, error_code) || error_code)
    {
        return false;
    }

    const std::filesystem::path compile_commands =
        locate_compile_commands(canonical, error_code);
    if (error_code)
    {
        return false;
    }

    std::vector<SCompileCommand> commands;
    if (!CCompileCommandsParser::parse(compile_commands, commands))
    {
        return false;
    }

    const std::string canonical_text = canonical.string();
    const std::string full_hash = CHash::sha256_hex(canonical_text);

    const std::filesystem::path db_path =
        m_config_manager.db_dir() / full_hash.substr(0, k_db_hash_length) / k_index_db_name;

    std::filesystem::create_directories(db_path.parent_path(), error_code);
    if (error_code)
    {
        return false;
    }

    SProject project;
    project.m_project_id = std::string(k_project_id_prefix) + full_hash.substr(0, k_project_id_hash_length);
    project.m_canonical_path = canonical;
    project.m_db_path = db_path;
    project.m_name = canonical.filename().string();
    project.m_last_opened_at = now_unix_seconds();

    CSchemaMigrator registry_migrator;
    if (!registry_migrator.migrate(m_config_manager.config().m_registry_path.string()))
    {
        return false;
    }

    if (!m_registry.open_db())
    {
        return false;
    }

    const bool registered = m_registry.upsert(project);
    m_registry.close_db();
    if (!registered)
    {
        return false;
    }

    CSchemaMigrator migrator;
    if (!migrator.migrate(db_path.string()))
    {
        return false;
    }

    CSQLiteIndex index(db_path);
    if (!index.open(canonical))
    {
        return false;
    }

    std::unique_ptr<IClang> clang = create_clang_backend(m_config_manager.config().m_clang_path);
    if (clang == nullptr)
    {
        index.close_db();
        return false;
    }

    const SIndexStatus existing = index.status();
    if (existing.m_state != EIndexState::Ready || existing.m_symbol_count == 0)
    {
        if (!index.store_inventory(commands))
        {
            index.close_db();
            return false;
        }

        if (!index.warm(*clang))
        {
            index.close_db();
            return false;
        }
    }
    else
    {
        // Индекс уже построен — обновляем только при фактическом устаревании (Этап 4.1).
        if (!index.refresh_if_stale(*clang, compile_commands.string()))
        {
            index.close_db();
            return false;
        }
    }

    index.set_compile_commands_fingerprint(compile_commands.string());

    m_current.m_project = project;
    m_current.m_index_status = index.status();
    index.close_db();

    m_has_current = true;
    return true;
}

std::filesystem::path CProjectManager::locate_compile_commands(const std::filesystem::path& project_root,
                                                               std::error_code& out_error) const
{
    const SCompileCommandsLocation location =
        CCompileCommandsLocator::resolve(project_root,
                                         m_compile_commands_path,
                                         m_config_manager.config().m_compile_commands_path,
                                         m_config_manager.config().m_default_build);

    if (!location.m_found)
    {
        out_error = std::make_error_code(std::errc::no_such_file_or_directory);
        return {};
    }

    out_error = std::error_code{};
    return location.m_path;
}

void CProjectManager::close()
{
    m_has_current = false;
    m_current = SProjectContext{};
}

bool CProjectManager::has_current() const
{
    return m_has_current;
}

const SProjectContext& CProjectManager::current() const
{
    return m_current;
}

bool CProjectManager::refresh_index()
{
    if (!m_has_current)
    {
        return false;
    }

    CSQLiteIndex index(m_current.m_project.m_db_path);
    if (!index.open(m_current.m_project.m_canonical_path))
    {
        return false;
    }

    std::unique_ptr<IClang> clang = create_clang_backend(m_config_manager.config().m_clang_path);
    if (clang == nullptr)
    {
        index.close_db();
        return false;
    }

    std::error_code error_code;
    const std::filesystem::path compile_commands =
        locate_compile_commands(m_current.m_project.m_canonical_path, error_code);
    const std::string compile_commands_text = error_code ? std::string() : compile_commands.string();

    const bool ok = index.refresh_if_stale(*clang, compile_commands_text);
    if (ok && !compile_commands_text.empty())
    {
        index.set_compile_commands_fingerprint(compile_commands_text);
    }
    m_current.m_index_status = index.status();
    index.close_db();
    return ok;
}

bool CProjectManager::rebuild_index()
{
    if (!m_has_current)
    {
        return false;
    }

    std::error_code error_code;
    const std::filesystem::path compile_commands =
        locate_compile_commands(m_current.m_project.m_canonical_path, error_code);
    if (error_code)
    {
        return false;
    }

    std::vector<SCompileCommand> commands;
    if (!CCompileCommandsParser::parse(compile_commands, commands))
    {
        return false;
    }

    CSQLiteIndex index(m_current.m_project.m_db_path);
    if (!index.open(m_current.m_project.m_canonical_path))
    {
        return false;
    }

    std::unique_ptr<IClang> clang = create_clang_backend(m_config_manager.config().m_clang_path);
    if (clang == nullptr)
    {
        index.close_db();
        return false;
    }

    const bool ok = index.rebuild(*clang, commands);
    m_current.m_index_status = index.status();
    index.close_db();
    return ok;
}

bool CProjectManager::save_session() const
{
    if (!m_has_current)
    {
        return false;
    }

    const std::filesystem::path session_path = m_config_manager.agent_dir() / "session";
    std::ofstream output(session_path);
    if (!output.is_open())
    {
        return false;
    }

    output << m_current.m_project.m_canonical_path.string() << '\n';
    output.close();
    return output.good();
}

bool CProjectManager::restore_session()
{
    const std::filesystem::path session_path = m_config_manager.agent_dir() / "session";

    std::error_code error_code;
    if (!std::filesystem::exists(session_path, error_code) || error_code)
    {
        return true;
    }

    std::ifstream input(session_path);
    if (!input.is_open())
    {
        return true;
    }

    std::string path;
    std::getline(input, path);
    if (path.empty())
    {
        return true;
    }

    return open(path);
}

void CProjectManager::clear_session() const
{
    const std::filesystem::path session_path = m_config_manager.agent_dir() / "session";
    std::error_code error_code;
    std::filesystem::remove(session_path, error_code);
}

std::vector<SSymbol> CProjectManager::find_symbols(const std::string& name) const
{
    if (!m_has_current || name.empty())
    {
        return {};
    }

    CSQLiteIndex index(m_current.m_project.m_db_path);
    if (!index.open(m_current.m_project.m_canonical_path))
    {
        return {};
    }

    const std::vector<SSymbol> symbols = index.find_symbols_by_name(name);
    index.close_db();
    return symbols;
}

std::vector<SProject> CProjectManager::list() const
{
    const std::filesystem::path registry_path = m_config_manager.config().m_registry_path;

    CSchemaMigrator registry_migrator;
    if (!registry_migrator.migrate(registry_path.string()))
    {
        return {};
    }

    CProjectRegistry registry(registry_path);
    if (!registry.open_db())
    {
        return {};
    }

    const std::vector<SProject> projects = registry.list();
    registry.close_db();
    return projects;
}

} // namespace fcpp
