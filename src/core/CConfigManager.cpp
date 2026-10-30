/**
 * @file CConfigManager.cpp
 * @brief Реализация менеджмента конфигурации fcpp-agent.
 *
 * Формат agent.conf — в стиле FreeBSD rc.conf (key="value", комментарии "#",
 * boolean-like YES/NO). JSON в конфиге не используется (требование §11).
 */

#include "core/CConfigManager.h"
#include "fcpp-agent/SLlmRoleConfig.h"

#include <algorithm>
#include <cctype>
#include <limits>
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

/** Parse an unsigned size from config without overflow. */
bool parse_size_value(const std::string& value, std::size_t& result)
{
    if (value.empty())
    {
        return false;
    }
    std::size_t parsed = 0;
    for (const unsigned char character : value)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10U)
        {
            return false;
        }
        parsed = parsed * 10U + digit;
    }
    result = parsed;
    return true;
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
        {"registry_path", registry_path.string()},
        {"db_dir", m_db_dir.string()},
        {"default_build", "build"},
        {"compile_commands_path", ""},
        {"tool_clang_path", ""},
        {"tool_cmake_path", ""},
        {"tool_ninja_path", ""},
        {"tool_clang_tidy_path", ""},
        {"enable_tidy", "YES"},
        {"allow_read", "YES"},
        {"allow_write", "YES"},
        {"allow_exec", "YES"},
        {"llm_coder_alias", ""}, {"llm_thinker_alias", ""},
        // Smart roles are remote: the endpoint cannot be probed, so the window
        // is a hard configuration contract. 56K is the working limit here.
        {"llm_coder_smart_api", ""}, {"llm_coder_smart_model", ""}, {"llm_coder_smart_key", ""},
        {"llm_coder_smart_type", "openai-compat"}, {"llm_coder_smart_alias", ""},
        {"llm_coder_smart_context_window_tokens", "57344"}, {"llm_coder_smart_output_reserve_tokens", "8192"},
        {"llm_coder_local_api", ""}, {"llm_coder_local_model", ""}, {"llm_coder_local_key", ""},
        {"llm_coder_local_type", "openai-compat"}, {"llm_coder_local_alias", ""},
        {"llm_coder_local_context_window_tokens", "0"}, {"llm_coder_local_output_reserve_tokens", "4096"},
        {"llm_thinker_smart_api", ""}, {"llm_thinker_smart_model", ""}, {"llm_thinker_smart_key", ""},
        {"llm_thinker_smart_type", "openai-compat"}, {"llm_thinker_smart_alias", ""},
        {"llm_thinker_smart_context_window_tokens", "57344"}, {"llm_thinker_smart_output_reserve_tokens", "8192"},
        {"llm_thinker_local_api", ""}, {"llm_thinker_local_model", ""}, {"llm_thinker_local_key", ""},
        {"llm_thinker_local_type", "openai-compat"}, {"llm_thinker_local_alias", ""},
        {"llm_thinker_local_context_window_tokens", "0"}, {"llm_thinker_local_output_reserve_tokens", "4096"}
    };

    // Файл отсутствует — пишем полный шаблон с комментариями.
    if (!std::filesystem::exists(m_config_path, error_code))
    {
        std::ostringstream content;
        content << "# fcpp-agent configuration\n\n";
        content << "# --- Paths ---\n";
        content << "registry_path=\"" << defaults[0].second << "\"\n";
        content << "db_dir=\"" << defaults[1].second << "\"\n";
        content << "default_build=\"" << defaults[2].second << "\"\n";
        content << "compile_commands_path=\"" << defaults[3].second << "\"\n\n";
        content << "# --- Tools (empty path = search in PATH) ---\n";
        content << "tool_clang_path=\"" << defaults[4].second << "\"\n";
        content << "tool_cmake_path=\"" << defaults[5].second << "\"\n";
        content << "tool_ninja_path=\"" << defaults[6].second << "\"\n";
        content << "tool_clang_tidy_path=\"" << defaults[7].second << "\"\n";
        content << "enable_tidy=\"" << defaults[8].second << "\"\n\n";
        content << "# --- Permissions (closed security model, §47) ---\n";
        content << "allow_read=\"" << defaults[9].second << "\"\n";
        content << "allow_write=\"" << defaults[10].second << "\"\n";
        content << "allow_exec=\"" << defaults[11].second << "\"\n\n";
        content << "# --- LLM roles ---\n";
        content << "# alias takes priority: when non-empty, api/model/key/type are ignored.\n";
        content << "# Without alias, api is mandatory (model is optional for local servers).\n";
        content << "# llm_coder_alias / llm_thinker_alias apply to both roles of the group.\n";
        content << "# type: openai-compat | claude\n\n";
        content << "llm_coder_smart_api=\"\"\n";
        content << "llm_coder_smart_model=\"\"\n";
        content << "llm_coder_smart_key=\"\"\n";
        content << "llm_coder_smart_type=\"openai-compat\"\n";
        content << "llm_coder_smart_alias=\"\"\n";
        content << "llm_coder_smart_context_window_tokens=\"57344\"\n";
        content << "llm_coder_smart_output_reserve_tokens=\"8192\"\n\n";
        content << "llm_coder_local_api=\"\"\n";
        content << "llm_coder_local_model=\"\"\n";
        content << "llm_coder_local_key=\"\"\n";
        content << "llm_coder_local_type=\"openai-compat\"\n";
        content << "llm_coder_local_alias=\"\"\n";
        content << "llm_coder_local_context_window_tokens=\"0\"\n";
        content << "llm_coder_local_output_reserve_tokens=\"4096\"\n\n";
        content << "llm_thinker_smart_api=\"\"\n";
        content << "llm_thinker_smart_model=\"\"\n";
        content << "llm_thinker_smart_key=\"\"\n";
        content << "llm_thinker_smart_type=\"openai-compat\"\n";
        content << "llm_thinker_smart_alias=\"\"\n";
        content << "llm_thinker_smart_context_window_tokens=\"57344\"\n";
        content << "llm_thinker_smart_output_reserve_tokens=\"8192\"\n\n";
        content << "llm_thinker_local_api=\"\"\n";
        content << "llm_thinker_local_model=\"\"\n";
        content << "llm_thinker_local_key=\"\"\n";
        content << "llm_thinker_local_type=\"openai-compat\"\n";
        content << "llm_thinker_local_alias=\"\"\n";
        content << "llm_thinker_local_context_window_tokens=\"0\"\n";
        content << "llm_thinker_local_output_reserve_tokens=\"4096\"\n";

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

    // Пути/сборка.
    const std::string db_dir_value = value_or_empty(entries, "db_dir");
    m_config.m_db_dir = db_dir_value.empty() ? m_db_dir : std::filesystem::path(db_dir_value);
    const std::string build_value = value_or_empty(entries, "default_build");
    m_config.m_default_build = build_value.empty() ? "build" : build_value;

    // Логические роли LLM: api/model/key/type либо alias (alias приоритетнее).
    auto read_size = [&entries](const std::string& key, std::size_t fallback,
                                std::size_t& output) -> bool
    {
        const auto iterator = entries.find(key);
        if (iterator == entries.end())
        {
            output = fallback;
            return true;
        }
        return parse_size_value(iterator->second, output);
    };
    // Group aliases (llm_coder_alias / llm_thinker_alias) apply to both roles of
    // the group; a per-role alias always wins over the group alias.
    const std::string coder_group_alias = value_or_empty(entries, "llm_coder_alias");
    const std::string thinker_group_alias = value_or_empty(entries, "llm_thinker_alias");

    auto load_llm_role = [&entries, &read_size](SLlmModelRole& role,
                                                const std::string& prefix,
                                                const std::string& group_alias) -> bool
    {
        role.m_api = value_or_empty(entries, prefix + "_api");
        role.m_model = value_or_empty(entries, prefix + "_model");
        role.m_key = value_or_empty(entries, prefix + "_key");
        role.m_type = value_or_empty(entries, prefix + "_type");
        role.m_alias = value_or_empty(entries, prefix + "_alias");
        if (role.m_alias.empty())
        {
            role.m_alias = group_alias;
        }
        if (role.m_type.empty())
        {
            role.m_type = "openai-compat";
        }
        if (!read_size(prefix + "_context_window_tokens", 0, role.m_context_window_tokens)
            || !read_size(prefix + "_output_reserve_tokens", 4096, role.m_output_reserve_tokens))
        {
            return false;
        }
        if (role.m_output_reserve_tokens == 0)
        {
            role.m_output_reserve_tokens = 4096;
        }
        return true;
    };

    if (!load_llm_role(m_config.m_llm_roles.at(ELlmRole::CoderSmart),
                       "llm_coder_smart", coder_group_alias)
        || !load_llm_role(m_config.m_llm_roles.at(ELlmRole::CoderLocal),
                          "llm_coder_local", coder_group_alias)
        || !load_llm_role(m_config.m_llm_roles.at(ELlmRole::ThinkerSmart),
                          "llm_thinker_smart", thinker_group_alias)
        || !load_llm_role(m_config.m_llm_roles.at(ELlmRole::ThinkerLocal),
                          "llm_thinker_local", thinker_group_alias))
    {
        return false;
    }

    std::string alias_error;
    if (!resolve_llm_role_aliases(m_config.m_llm_roles, alias_error))
    {
        return false;
    }

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
