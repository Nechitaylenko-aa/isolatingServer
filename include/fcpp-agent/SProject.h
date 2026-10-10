/**
 * @file SProject.h
 * @brief Структуры данных проекта (ADR-03).
 */

#ifndef FCPP_AGENT_SPROJECT_H
#define FCPP_AGENT_SPROJECT_H

#include <cstdint>
#include <filesystem>
#include <string>

#include "fcpp-agent/IIndex.h"
#include "fcpp-agent/CProjectProfile.h"

namespace fcpp
{

/**
 * @brief Зарегистрированный проект (запись реестра registry_project).
 *
 * project_id — непрозрачный идентификатор, не равный пути (§89.3.1).
 * canonical_path — канонизированный абсолютный путь CMake-корня.
 * db_path — путь к per-project базе индекса index.db.
 */
struct SProject
{
    std::string m_project_id;                ///< Непрозрачный идентификатор проекта.
    std::filesystem::path m_canonical_path;  ///< Канонический путь CMake-корня.
    std::filesystem::path m_db_path;         ///< Путь к базе индекса index.db.
    std::string m_name;                       ///< Отображаемое имя проекта.
    std::int64_t m_last_opened_at = 0;        ///< Время последнего открытия (unix time).
};

/**
 * @brief Контекст текущего открытого проекта.
 */
struct SProjectContext
{
    SProject m_project;         ///< Проект.
    SIndexStatus m_index_status;///< Снимок состояния индекса.
    CProjectProfile m_profile; ///< Паспорт текущего проекта (пока только в памяти).
};

} // namespace fcpp

#endif // FCPP_AGENT_SPROJECT_H
