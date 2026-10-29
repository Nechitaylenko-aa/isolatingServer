/**
 * @file SConfig.h
 * @brief Структура конфигурации fcpp-agent.
 */

#ifndef FCPP_AGENT_SCONFIG_H
#define FCPP_AGENT_SCONFIG_H

#include <filesystem>
#include <string>

#include "fcpp-agent/SLlmRoleConfig.h"

namespace fcpp
{

/**
 * @brief Параметры конфигурации агента (agent.conf).
 *
 * Модели описаны только логическими ролями (SLlmRoleSet). Отдельной
 * Lite/Main/Remote-пирамиды нет: выбор роли выполняет вызывающая сущность.
 */
struct SConfig
{
    std::filesystem::path m_clang_path;      ///< Путь к clang/libclang (пусто — поиск в PATH).
    std::filesystem::path m_cmake_path;      ///< Путь к cmake (пусто — поиск в PATH).
    std::filesystem::path m_ninja_path;      ///< Путь к ninja (пусто — поиск в PATH).
    std::filesystem::path m_clang_tidy_path; ///< Путь к clang-tidy (пусто — поиск в PATH).
    bool m_enable_tidy = true;               ///< Включён ли статический анализ (clang-tidy).
    std::filesystem::path m_registry_path;   ///< Путь к глобальному реестру проектов.
    std::filesystem::path m_compile_commands_path; ///< Путь к compile_commands.json (пусто — стандарт).
    bool m_allow_read = true;                ///< Разрешает чтение проекта и индекса (§47).
    bool m_allow_write = true;               ///< Разрешает изменение исходников (§47).
    bool m_allow_exec = true;                ///< Разрешает запуск build/tests/tidy (§47).

    std::filesystem::path m_db_dir;          ///< Каталог баз данных агента (вне проектов).
    std::string m_default_build = "build";   ///< Имя build-каталога по умолчанию (§2.3).

    SLlmRoleSet m_llm_roles; ///< Логические роли LLM.
};

} // namespace fcpp

#endif // FCPP_AGENT_SCONFIG_H
