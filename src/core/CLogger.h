/**
 * @file CLogger.h
 * @brief Файловый структурированный логгер (§60).
 *
 * Пишет строки вида "TIMESTAMP [LEVEL] message" в файл вне каталога проекта.
 */

#ifndef FCPP_AGENT_CLOGGER_H
#define FCPP_AGENT_CLOGGER_H

#include <filesystem>
#include <mutex>

#include <nlohmann/json.hpp>
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

    /** @brief Write one structured JSONL event. */
    void log_event(ELogLevel level, const std::string& event,
                   const nlohmann::json& fields = nlohmann::json::object());

private:
    std::filesystem::path m_log_path; ///< Путь к файлу лога.
    ELogLevel m_min_level;            ///< Минимальный уровень.
    std::mutex m_mutex;                ///< Prevent interleaved records.
};

} // namespace fcpp

#endif // FCPP_AGENT_CLOGGER_H
