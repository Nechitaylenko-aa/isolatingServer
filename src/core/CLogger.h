/**
 * @file CLogger.h
 * @brief Файловый структурированный логгер (§60).
 *
 * Пишет строки вида "TIMESTAMP [LEVEL] message" в файл вне каталога проекта.
 */

#ifndef FCPP_AGENT_CLOGGER_H
#define FCPP_AGENT_CLOGGER_H

#include <filesystem>
#include <string>

#include "fcpp-agent/ILogger.h"

namespace fcpp
{

/**
 * @brief Логгер в файл.
 */
class CLogger final : public ILogger
{
public:
    /**
     * @brief Конструктор.
     * @param log_dir Каталог для логов (создаётся при необходимости).
     * @param min_level Минимальный уровень, который пишется.
     */
    CLogger(std::filesystem::path log_dir, ELogLevel min_level);

    /**
     * @brief Деструктор.
     */
    ~CLogger() override = default;

    /**
     * @brief Записать сообщение, если его уровень не ниже порога.
     * @param level Уровень.
     * @param message Сообщение.
     */
    void log(ELogLevel level, const std::string& message) override;

    /**
     * @brief Получить путь к файлу лога.
     * @return Путь к файлу.
     */
    const std::filesystem::path& path() const;

private:
    std::filesystem::path m_log_path; ///< Путь к файлу лога.
    ELogLevel m_min_level;            ///< Минимальный уровень.
};

} // namespace fcpp

#endif // FCPP_AGENT_CLOGGER_H
