/**
 * @file SSnapshot.h
 * @brief Типы для теневого git-снапшота проекта (decisions.md №22).
 */

#ifndef FCPP_AGENT_SSNAPSHOT_H
#define FCPP_AGENT_SSNAPSHOT_H

#include <cstdint>
#include <string>

namespace fcpp
{

/** @brief Один снапшот в истории теневого репозитория проекта. */
struct SSnapshotEntry
{
    std::string m_sha;       ///< Полный SHA коммита.
    std::string m_message;   ///< Сообщение коммита (содержит request_id).
    std::string m_timestamp; ///< ISO-8601, как отдаёт git.
};

} // namespace fcpp

#endif // FCPP_AGENT_SSNAPSHOT_H
