/**
 * @file CConfigManager.cpp
 * @brief Реализация менеджмента конфигурации fcpp-agent.
 *
 * Формат agent.conf — в стиле FreeBSD rc.conf (key="value", комментарии "#",
 * boolean-like YES/NO). JSON в конфиге не используется (требование §11).
 */

#include "core/CConfigManager.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fcpp
{

namespace
{

/** @brief Имя каталога конфигурации агента. */
constexpr const char* k_agent_dir_name = ".fcpp-agent";

/** @brief Имя файла конфигурации. */
constexpr const char* k_config_file_name = "agent.conf";

/** @brief Имя каталога баз данных. */
constexpr const char* k_db_dir_name = "db";

/** @brief Имя каталога проектов. */
constexpr const char* k_projects_dir_name = "projects";

/** @brief Имя файла реестра проектов. */
constexpr const char* k_registry_file_name = "registry.db";

/**
 * @brief Удалить пробельные символы по краям строки.
 * @param text Исходная строка.
 * @return Строка без ведущих и замыкающих пробелов.
 */
std::string trim(const std::string& text)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string::npos)
    {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

/**
 * @brief Привести строку к верхнему регистру (для YES/NO).
 * @param text Исходная строка.
 * @return Строка в верхнем регистре.
 */
std::string to_upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::toupper(character));
                   });
    return text;
}

/**
 * @brief Разобрать одну строку rc.conf в пару ключ/значение.
 * @param raw Исходная строка.
 * @param key Выходной ключ.
 * @param value Выходное значение (без кавычек).
 * @return true если строка содержит корректную пару key="value".
 */
bool parse_config_line(const std::string& raw, std::string& key, std::string& value)
{
    std::string line = raw;

    const std::size_t comment = line.find('#');
    if (comment != std::string::npos)
    {
        line = line.substr(0, comment);
    }

    line = trim(line);
    if (line.empty())
    {
        return false;
    }

    const std::size_t separator = line.find('=');
    if (separator == std::string::npos)
    {
        return false;
    }

    key = trim(line.substr(0, separator));
    value = trim(line.substr(separator + 1));

    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        value = value.substr(1, value.size() - 2);
    }

    return !key.empty();
}

/**
 * @brief Получить значение по ключу или пустую строку.
 * @param entries Словарь параметров.
 * @param key Искомый ключ.
 * @return Значение параметра либо пустая строка.
 */
std::string value_or_empty(const std::map<std::string, std::string>& entries,
                           const std::string& key)
{
    const auto iterator = entries.find(key);
    if (iterator == entries.end())
    {
        return {};
    }
    return iterator->second;
}

} // namespace

CConfigManager::CConfigManager()
{
    const std::filesystem::path home = resolve_home_dir();
    m_agent_dir = home / k_agent_dir_name;
    m_config_path = m_agent_dir / k_config_file_name;
    m_db_dir = m_agent_dir / k_db_dir_name;
    m_projects_dir = m_agent_dir / k_projects_dir_name;
}

std::filesystem::path CConfigManager::resolve_home_dir()
{
    const char* home_env = std::getenv("HOME");
    if (home_env != nullptr && home_env[0] != '\0')
    {
        return std::filesystem::path(home_env);
    }

    const char* profile_env = std::getenv("USERPROFILE");
    if (profile_env != nullptr && profile_env[0] != '\0')
    {
        return std::filesystem::path(profile_env);
    }

    return std::filesystem::current_path();
}

bool CConfigManager::bootstrap()
{
    std::error_code error_code;

    std::filesystem::create_directories(m_agent_dir, error_code);
    if (error_code)
    {
        return false;
    }

    std::filesystem::create_directories(m_db_dir, error_code);
    if (error_code)
    {
        return false;
    }

    std::filesystem::create_directories(m_projects_dir, error_code);
    if (error_code)
    {
        return false;
    }

    const std::filesystem::path registry_path = m_projects_dir / k_registry_file_name;

    // Каноническая схема конфига: порядок и значения по умолчанию.
    std::vector<std::pair<std::string, std::string>> defaults =
    {
        {"model_main", "http://127.0.0.1:8080"},
        {"model_main_name", ""},
        {"model_light", "same_as_main"},
        {"model_light_name", ""},
        {"model_remote_url", "https://routerai.ru/api/v1"},
        {"model_remote_type", "openai-compat"},
        {"model_remote_name", ""},
        {"model_remote_key", ""},
        {"db_dir", m_db_dir.string()},
        {"default_build", "build"},
        {"registry_path", registry_path.string()},
        {"compile_commands_path", ""},
        {"tool_clang_path", ""},
        {"tool_cmake_path", ""},
        {"tool_ninja_path", ""},
        {"tool_clang_tidy_path", ""},
        {"enable_tidy", "YES"},
        {"allow_read", "YES"},
        {"allow_write", "YES"},
        {"allow_exec", "YES"},
        {"allow_subtasks", "NO"}
    };

    // Файл отсутствует — пишем полный шаблон с комментариями.
    if (!std::filesystem::exists(m_config_path, error_code))
    {
        std::ostringstream content;
        content << "# fcpp-agent configuration\n\n";
        content << "# --- Models (§6/§7) ---\n";
        content << "model_main=\"" << defaults[0].second << "\"\n";
        content << "model_main_name=\"" << defaults[1].second << "\"\n";
        content << "model_light=\"" << defaults[2].second << "\"\n";
        content << "model_light_name=\"" << defaults[3].second << "\"\n\n";
        content << "# remote — удалённая модель (задел, пока не используется)\n";
        content << "model_remote_url=\"" << defaults[4].second << "\"\n";
        content << "model_remote_type=\"" << defaults[5].second << "\"\n";
        content << "model_remote_name=\"" << defaults[6].second << "\"\n";
        content << "model_remote_key=\"" << defaults[7].second << "\"\n\n";
        content << "# --- Paths ---\n";
        content << "db_dir=\"" << defaults[8].second << "\"\n";
        content << "default_build=\"" << defaults[9].second << "\"\n";
        content << "registry_path=\"" << defaults[10].second << "\"\n";
        content << "compile_commands_path=\"" << defaults[11].second << "\"\n\n";
        content << "# --- Tools (empty path = search in PATH) ---\n";
        content << "tool_clang_path=\"" << defaults[12].second << "\"\n";
        content << "tool_cmake_path=\"" << defaults[13].second << "\"\n";
        content << "tool_ninja_path=\"" << defaults[14].second << "\"\n";
        content << "tool_clang_tidy_path=\"" << defaults[15].second << "\"\n";
        content << "enable_tidy=\"" << defaults[16].second << "\"\n\n";
        content << "# --- Permissions (closed security model, §47) ---\n";
        content << "allow_read=\"" << defaults[17].second << "\"\n";
        content << "allow_write=\"" << defaults[18].second << "\"\n";
        content << "allow_exec=\"" << defaults[19].second << "\"\n";
        content << "allow_subtasks=\"" << defaults[20].second << "\"\n";

        std::ofstream output(m_config_path);
        if (!output.is_open())
        {
            return false;
        }
        output << content.str();
        output.close();
        return output.good();
    }

    // Файл существует — дополняем недостающими ключами, не трогая пользовательские значения.
    std::set<std::string> existing_keys;
    {
        std::ifstream input(m_config_path);
        std::string line;
        while (std::getline(input, line))
        {
            std::string key;
            std::string value;
            if (parse_config_line(line, key, value))
            {
                existing_keys.insert(key);
            }
        }
    }

    std::ostringstream appended;
    for (const std::pair<std::string, std::string>& entry : defaults)
    {
        if (existing_keys.find(entry.first) == existing_keys.end())
        {
            appended << entry.first << "=\"" << entry.second << "\"\n";
        }
    }

    if (appended.str().empty())
    {
        return true;
    }

    std::ofstream output(m_config_path, std::ios::app);
    if (!output.is_open())
    {
        return false;
    }
    output << "\n# --- added by fcpp-agent (new keys) ---\n";
    output << appended.str();
    output.close();
    return output.good();
}

bool CConfigManager::reload_config() const
{
    return load();
}

bool CConfigManager::load() const
{
    std::error_code error_code;
    if (!std::filesystem::exists(m_config_path, error_code))
    {
        return false;
    }

    std::ifstream input(m_config_path);
    if (!input.is_open())
    {
        return false;
    }

    std::map<std::string, std::string> entries;
    std::string line;
    while (std::getline(input, line))
    {
        std::string key;
        std::string value;
        if (parse_config_line(line, key, value))
        {
            entries[key] = value;
        }
    }

    const auto tidy_iterator = entries.find("enable_tidy");
    if (tidy_iterator == entries.end())
    {
        return false;
    }
    const std::string tidy_value = to_upper(tidy_iterator->second);
    if (tidy_value != "YES" && tidy_value != "NO")
    {
        return false;
    }

    const auto registry_iterator = entries.find("registry_path");
    if (registry_iterator == entries.end() || registry_iterator->second.empty())
    {
        return false;
    }

    // Permissions (§47): отсутствие ключа — безопасный дефолт (закрытая модель).
    auto read_bool = [&entries](const std::string& key, bool fallback) -> bool
    {
        const auto iterator = entries.find(key);
        if (iterator == entries.end())
        {
            return fallback;
        }
        const std::string value = to_upper(iterator->second);
        if (value == "YES")
        {
            return true;
        }
        if (value == "NO")
        {
            return false;
        }
        return fallback;
    };

    m_config.m_clang_path = value_or_empty(entries, "tool_clang_path");
    m_config.m_cmake_path = value_or_empty(entries, "tool_cmake_path");
    m_config.m_ninja_path = value_or_empty(entries, "tool_ninja_path");
    m_config.m_clang_tidy_path = value_or_empty(entries, "tool_clang_tidy_path");
    m_config.m_enable_tidy = (tidy_value == "YES");
    m_config.m_registry_path = registry_iterator->second;
    m_config.m_compile_commands_path = value_or_empty(entries, "compile_commands_path");
    m_config.m_allow_read = read_bool("allow_read", true);
    m_config.m_allow_write = read_bool("allow_write", true);
    m_config.m_allow_exec = read_bool("allow_exec", true);
    m_config.m_allow_subtasks = read_bool("allow_subtasks", false);

    // Пути/сборка.
    const std::string db_dir_value = value_or_empty(entries, "db_dir");
    m_config.m_db_dir = db_dir_value.empty() ? m_db_dir : std::filesystem::path(db_dir_value);
    const std::string build_value = value_or_empty(entries, "default_build");
    m_config.m_default_build = build_value.empty() ? "build" : build_value;

    // Модели (§6/§7).
    m_config.m_model_main.m_url = value_or_empty(entries, "model_main");
    m_config.m_model_main.m_name = value_or_empty(entries, "model_main_name");
    m_config.m_model_light.m_url = value_or_empty(entries, "model_light");
    m_config.m_model_light.m_name = value_or_empty(entries, "model_light_name");
    m_config.m_model_remote.m_url = value_or_empty(entries, "model_remote_url");
    m_config.m_model_remote.m_type = value_or_empty(entries, "model_remote_type");
    m_config.m_model_remote.m_name = value_or_empty(entries, "model_remote_name");
    m_config.m_model_remote.m_key = value_or_empty(entries, "model_remote_key");
    m_config.m_remote_type = remote_type_from_name(m_config.m_model_remote.m_type);

    return true;
}

const SConfig& CConfigManager::config() const
{
    return m_config;
}

std::filesystem::path CConfigManager::agent_dir() const
{
    return m_agent_dir;
}

std::filesystem::path CConfigManager::db_dir() const
{
    return m_db_dir;
}

std::filesystem::path CConfigManager::projects_dir() const
{
    return m_projects_dir;
}

std::filesystem::path CConfigManager::config_path() const
{
    return m_config_path;
}

} // namespace fcpp
