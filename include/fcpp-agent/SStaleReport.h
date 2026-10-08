/**
 * @file SStaleReport.h
 * @brief Отчёт об устаревании индекса (ADR-05, Этап 4.1).
 */

#ifndef FCPP_AGENT_SSTALEREPORT_H
#define FCPP_AGENT_SSTALEREPORT_H

#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Причина устаревания индекса.
 */
enum class EStaleReason
{
    None,               ///< Индекс актуален.
    EnvironmentChange,  ///< Изменилось окружение (compile_commands/backend) — нужен полный rebuild.
    ContentChange,      ///< Изменилось содержимое файлов — возможен точечный refresh.
    Missing,            ///< Активной generation нет.
    Count               ///< Маркер количества.
};

/**
 * @brief Результат детекции устаревания.
 */
struct SStaleReport
{
    bool m_valid = false;                       ///< Активная generation существует.
    EStaleReason m_reason = EStaleReason::None; ///< Причина устаревания.
    std::vector<std::string> m_dirty_tus;       ///< Translation units для переиндексации.
    std::vector<std::string> m_removed_files;   ///< Файлы, исчезнувшие с диска.

    /**
     * @brief Требуется ли обновление индекса.
     * @return true если индекс устарел.
     */
    bool is_stale() const
    {
        return m_reason != EStaleReason::None;
    }
};

/**
     * @brief Получить машинное имя причины устаревания.
     * @param reason Причина.
     * @return Текстовое имя.
     */
const char* stale_reason_name(EStaleReason reason);

} // namespace fcpp

#endif // FCPP_AGENT_SSTALEREPORT_H
