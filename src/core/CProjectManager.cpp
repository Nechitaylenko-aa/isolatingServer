/**
 * @file CProjectManager.cpp
 * @brief Реализация управления проектами (ADR-03).
 */

#include "core/CProjectManager.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <system_error>
#include <vector>

#include <memory>
#include <string>
#include <utility>

#include <sqlite3.h>

#include "core/CConfigManager.h"
#include "core/CExecutor.h"
#include "core/CHash.h"
#include "core/CProjectPaths.h"
#include "core/CCompileCommandsLocator.h"
#include "core/CCompileCommandsParser.h"
#include "index/CClangFactory.h"
#include "index/CSQLiteIndex.h"
#include "index/CSchemaInitializer.h"

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

/** @brief Таймаут конфигурации CMake при первом открытии проекта (сек). */
constexpr std::uint32_t k_configure_timeout_seconds = 900;

/**
 * @brief Сконфигурировать проект, чтобы получить compile_commands.json.
 *
 * Выполняется только когда файла нет ни в одном разрешённом месте. Команда
 * детерминирована: `cmake -S <root> -B <build> -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.
 * Запуск идёт через CExecutor, то есть под закрытой моделью безопасности.
 *
 * @param project_root Канонический корень проекта.
 * @param build_dir Имя каталога сборки.
 * @param cmake_path Путь к cmake (пусто — из PATH).
 * @param out_error Причина сбоя.
 * @return true если CMake завершился успешно.
 */
bool configure_compile_commands(const std::filesystem::path& project_root,
                                const std::string& build_dir,
                                const std::filesystem::path& cmake_path,
                                std::string& out_error)
{
    const std::filesystem::path build_path = project_root / build_dir;
    std::error_code error_code;
    std::filesystem::create_directories(build_path, error_code);
    if (error_code)
    {
        out_error = "cannot create build directory: " + build_path.string();
        return false;
    }

    CExecutor executor;
    SCommandArgs args;
    args.m_tool = cmake_path.empty() ? "cmake" : cmake_path.string();
    args.m_arguments = {
        "-S", project_root.string(),
        "-B", build_path.string(),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    };
    args.m_working_dir = project_root;
    args.m_effect = EFFECT_WRITE_BUILD;
    args.m_timeout_seconds = k_configure_timeout_seconds;

    const SExecutionResult result = executor.execute(args, nullptr);
    if (!result.m_allowed)
    {
        out_error = "cmake configure denied by policy: " + result.m_stderr;
        return false;
    }
    if (result.m_exit_code != 0)
    {
        out_error = "cmake configure failed (exit "
            + std::to_string(result.m_exit_code) + "): " + result.m_stderr;
        return false;
    }
    return true;
}

/** @brief Read the project identity from CMakeLists.txt. */
bool read_cmake_project_name(const std::filesystem::path& cmake_lists, std::string& name)
{
    std::ifstream input(cmake_lists);
    if (!input.is_open()) return false;
    const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::regex pattern(R"((?:^|[\r\n])\s*project\s*\(\s*([A-Za-z0-9_.+-]+))",
                             std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_search(content, match, pattern) || match.size() < 2) return false;
    name = match[1].str();
    return !name.empty();
}

/**
 * @brief Текущее время в unix-секундах.
 * @return Число секунд с epoch.
 */
std::int64_t now_unix_seconds()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

/**
 * @brief Загрузить паспорт из БД проекта. Отсутствие актуальной таблицы — ошибка.
 */
bool load_project_profile(const std::filesystem::path& db_path, CProjectProfile& profile)
{
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(db_path.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "SELECT original_text, compact_text FROM project_profile WHERE id = 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }

    const int step_result = sqlite3_step(statement);
    bool ok = false;
    if (step_result == SQLITE_ROW)
    {
        const unsigned char* original = sqlite3_column_text(statement, 0);
        const unsigned char* compact = sqlite3_column_text(statement, 1);
        profile.restore(original != nullptr ? reinterpret_cast<const char*>(original) : std::string(),
                        compact != nullptr ? reinterpret_cast<const char*>(compact) : std::string());
        ok = true;
    }
    else if (step_result == SQLITE_DONE)
    {
        profile.restore({}, {});
        ok = true;
    }

    sqlite3_finalize(statement);
    sqlite3_close(db);
    return ok;
}

/** @brief Сохранить паспорт как одну атомарную запись в БД проекта. */
bool save_project_profile(const std::filesystem::path& db_path, const CProjectProfile& profile)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK)
    {
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
        return false;
    }

    sqlite3_busy_timeout(db, 10000);
    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO project_profile (id, original_text, compact_text) VALUES (1, ?1, ?2) "
        "ON CONFLICT(id) DO UPDATE SET original_text = excluded.original_text, "
        "compact_text = excluded.compact_text;";
    bool ok = sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK;
    if (ok)
    {
        const std::string& original = profile.original();
        const std::string& compact = profile.get();
        sqlite3_bind_text(statement, 1, original.c_str(), static_cast<int>(original.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, compact.c_str(), static_cast<int>(compact.size()), SQLITE_TRANSIENT);
        ok = sqlite3_step(statement) == SQLITE_DONE;
    }
    if (statement != nullptr)
    {
        sqlite3_finalize(statement);
    }

    if (ok)
    {
        ok = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
    }
    if (!ok)
    {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    sqlite3_close(db);
    return ok;
}

} // namespace

CProjectManager::CProjectManager(const CConfigManager& config_manager)
    : m_config_manager(config_manager)
    , m_registry(config_manager.config().m_registry_path)
    , m_compile_commands_path()
    , m_has_current(false)
    , m_current_opened(false)
    , m_current()
{
}

CProjectManager::~CProjectManager() = default;

std::filesystem::path CProjectManager::resolve_path(const std::filesystem::path& path) const
{
    if (!m_current_opened)
    {
        return {};
    }
    return CProjectPaths::resolve(path, m_current.m_project.m_canonical_path);
}

void CProjectManager::set_compile_commands_path(const std::filesystem::path& path)
{
    m_compile_commands_path = path;
}

bool CProjectManager::open(const std::filesystem::path& path, std::string* out_failure_stage)
{
    if (out_failure_stage != nullptr)
    {
        out_failure_stage->clear();
    }
    const auto fail = [out_failure_stage](const char* stage)
    {
        if (out_failure_stage != nullptr)
        {
            *out_failure_stage = stage;
        }
        return false;
    };

    std::error_code error_code;
    const std::filesystem::path canonical = std::filesystem::canonical(path, error_code);
    if (error_code)
    {
        return fail("canonicalize_project_root");
    }

    if (!std::filesystem::is_directory(canonical, error_code) || error_code)
    {
        return fail("validate_project_directory");
    }

    const std::filesystem::path cmake_lists = canonical / k_cmake_lists_name;
    if (!std::filesystem::exists(cmake_lists, error_code) || error_code)
    {
        return fail("find_cmake_lists");
    }

    std::string project_name;
    if (!read_cmake_project_name(cmake_lists, project_name))
    {
        return fail("read_cmake_project_name");
    }

    // Preflight: a freshly cloned project has no compile database. Generate it
    // deterministically once, instead of refusing to open the project.
    std::filesystem::path compile_commands =
        locate_compile_commands(canonical, error_code);
    if (error_code)
    {
        std::string configure_error;
        if (!configure_compile_commands(canonical, m_config_manager.config().m_default_build,
                                        m_config_manager.config().m_cmake_path,
                                        configure_error))
        {
            return fail("configure_compile_commands");
        }

        error_code.clear();
        compile_commands = locate_compile_commands(canonical, error_code);
        if (error_code || compile_commands.empty())
        {
            return fail("locate_compile_commands_after_configure");
        }
    }

    std::vector<SCompileCommand> commands;
    if (!CCompileCommandsParser::parse(compile_commands, commands))
    {
        return fail("parse_compile_commands");
    }

    // The project identity is stable across agent instances and folder moves.
    const std::string full_hash = CHash::sha256_hex(project_name);

    const std::string path_hash = CHash::sha256_hex(canonical.string());
    const std::filesystem::path db_path =
        m_config_manager.db_dir() / path_hash.substr(0, k_db_hash_length) / k_index_db_name;

    std::filesystem::create_directories(db_path.parent_path(), error_code);
    if (error_code)
    {
        return fail("create_index_directory");
    }

    SProject project;
    project.m_project_id = std::string(k_project_id_prefix) + full_hash.substr(0, k_project_id_hash_length);
    project.m_canonical_path = canonical;
    project.m_db_path = db_path;
    project.m_name = project_name;
    project.m_last_opened_at = now_unix_seconds();

    if (!CSchemaInitializer::initialize_or_validate(db_path))
    {
        return fail("initialize_index_schema");
    }

    const std::filesystem::path registry_path = m_config_manager.config().m_registry_path;
    if (!CSchemaInitializer::initialize_or_validate(registry_path))
    {
        return fail("initialize_project_registry_schema");
    }

    CSQLiteIndex index(db_path);
    if (!index.open(canonical))
    {
        return fail("open_index_database");
    }

    std::unique_ptr<IClang> clang = create_clang_backend(m_config_manager.config().m_clang_path);
    if (clang == nullptr)
    {
        index.close_db();
        return fail("create_clang_backend");
    }

    const SIndexStatus existing = index.status();
    if (existing.m_state != EIndexState::Ready || existing.m_symbol_count == 0)
    {
        if (!index.store_inventory(commands))
        {
            index.close_db();
            return fail("store_compile_inventory");
        }

        if (!index.warm(*clang))
        {
            index.close_db();
            return fail("warm_index");
        }
    }
    else
    {
        // Индекс уже построен — обновляем только при фактическом устаревании (Этап 4.1).
        if (!index.refresh_if_stale(*clang, compile_commands.string()))
        {
            index.close_db();
            return fail("refresh_stale_index");
        }
    }

    index.set_compile_commands_fingerprint(compile_commands.string());
    const SIndexStatus index_status = index.status();
    index.close_db();

    // Prepare the complete next context before replacing the current one.
    CProjectProfile project_profile;
    if (!load_project_profile(db_path, project_profile))
    {
        return fail("load_project_profile");
    }

    if (!m_registry.open_db())
    {
        return fail("open_project_registry");
    }
    const bool registered = m_registry.upsert(project);
    m_registry.close_db();
    if (!registered)
    {
        return fail("register_project");
    }

    m_current.m_project = project;
    m_current.m_index_status = index_status;
    m_current.m_profile = std::move(project_profile);
    m_has_current = true;
    m_current_opened = true;
    return true;
}

std::vector<std::string> CProjectManager::compile_arguments_for(
    const std::filesystem::path& file) const
{
    if (!m_current_opened)
    {
        return {};
    }

    std::error_code error_code;
    const std::filesystem::path database =
        locate_compile_commands(m_current.m_project.m_canonical_path, error_code);
    if (error_code)
    {
        return {};
    }

    std::vector<SCompileCommand> commands;
    if (!CCompileCommandsParser::parse(database, commands) || commands.empty())
    {
        return {};
    }

    const std::string target = file.string();
    const SCompileCommand* selected = nullptr;
    for (const SCompileCommand& command : commands)
    {
        if (command.m_file.string() == target)
        {
            selected = &command;
            break;
        }
    }

    // A header has no entry of its own: it is compiled inside some TU, so the
    // project flags (-I/-D/-std) of any entry apply to it as well.
    if (selected == nullptr)
    {
        selected = &commands.front();
    }

    std::vector<std::string> arguments = selected->m_arguments;
    if (arguments.empty() && !selected->m_command.empty())
    {
        std::string current;
        bool in_quotes = false;
        for (const char character : selected->m_command)
        {
            if (character == '"')
            {
                in_quotes = !in_quotes;
                continue;
            }
            if (!in_quotes && (character == ' ' || character == '\t'))
            {
                if (!current.empty())
                {
                    arguments.push_back(current);
                    current.clear();
                }
                continue;
            }
            current.push_back(character);
        }
        if (!current.empty())
        {
            arguments.push_back(current);
        }
    }

    // Drop the compiler binary, output flags and the source file itself:
    // libclang takes the file separately and needs only the flag set.
    std::vector<std::string> filtered;
    filtered.reserve(arguments.size());
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string& argument = arguments[index];
        if (index == 0 && argument.rfind("-", 0) != 0)
        {
            continue;
        }
        if (argument == "-o" || argument == "-MF" || argument == "-MT" || argument == "-MQ")
        {
            ++index;
            continue;
        }
        if (argument == "-c" || argument == target)
        {
            continue;
        }
        filtered.push_back(argument);
    }
    return filtered;
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
    m_current_opened = false;
    m_current = SProjectContext{};
}

bool CProjectManager::has_current() const
{
    return m_has_current;
}

bool CProjectManager::is_opened() const
{
    return m_current_opened;
}

const SProjectContext& CProjectManager::current() const
{
    return m_current;
}

CProjectProfile& CProjectManager::profile()
{
    return m_current.m_profile;
}

const CProjectProfile& CProjectManager::profile() const
{
    return m_current.m_profile;
}

bool CProjectManager::set_profile(std::string original_text)
{
    if (!m_current_opened)
    {
        return false;
    }

    CProjectProfile next_profile;
    next_profile.set(std::move(original_text));
    if (!save_project_profile(m_current.m_project.m_db_path, next_profile))
    {
        return false;
    }

    m_current.m_profile = std::move(next_profile);
    return true;
}

bool CProjectManager::refresh_index()
{
    if (!m_current_opened)
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
    if (!m_current_opened)
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
    if (!m_current_opened)
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

bool CProjectManager::restore_session_light()
{
    if (m_has_current)
    {
        return true;
    }

    const std::filesystem::path session_path = m_config_manager.agent_dir() / "session";

    std::error_code error_code;
    if (!std::filesystem::exists(session_path, error_code) || error_code)
    {
        return false;
    }

    std::ifstream input(session_path);
    if (!input.is_open())
    {
        return false;
    }

    std::string path;
    std::getline(input, path);
    if (path.empty())
    {
        return false;
    }

    // Идентификация проекта без open/warm/refresh (read-only).
    const std::filesystem::path canonical = std::filesystem::canonical(path, error_code);
    if (error_code)
    {
        return false;
    }

    std::string project_name;
    if (!read_cmake_project_name(canonical / k_cmake_lists_name, project_name))
    {
        return false;
    }
    const std::string full_hash = CHash::sha256_hex(project_name);
    const std::string path_hash = CHash::sha256_hex(canonical.string());
    const std::filesystem::path db_path =
        m_config_manager.db_dir() / path_hash.substr(0, k_db_hash_length) / k_index_db_name;

    SProject project;
    project.m_project_id = std::string(k_project_id_prefix) + full_hash.substr(0, k_project_id_hash_length);
    project.m_canonical_path = canonical;
    project.m_db_path = db_path;
    project.m_name = project_name;

    // Читаем состояние индекса и паспорт напрямую из БД (без индексации).
    SIndexStatus index_status;
    CProjectProfile project_profile;
    if (std::filesystem::exists(db_path, error_code) && !error_code)
    {
        CSQLiteIndex index(db_path);
        if (index.open_db())
        {
            index_status = index.status();
            index.close_db();
        }
        if (!load_project_profile(db_path, project_profile))
        {
            return false;
        }
    }

    m_current.m_project = project;
    m_current.m_index_status = index_status;
    m_current.m_profile = std::move(project_profile);
    m_has_current = true;
    m_current_opened = false;
    return true;
}

void CProjectManager::clear_session() const
{
    const std::filesystem::path session_path = m_config_manager.agent_dir() / "session";
    std::error_code error_code;
    std::filesystem::remove(session_path, error_code);
}

std::vector<SSymbol> CProjectManager::find_symbols(const std::string& name) const
{
    if (!m_current_opened || name.empty())
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

    if (!CSchemaInitializer::initialize_or_validate(registry_path))
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
