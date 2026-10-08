/**
 * @file CLogger.cpp
 * @brief Реализация файлового логгера (§60).
 */

#include "core/CLogger.h"

#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fcpp
{

namespace
{

/** @brief Имена уровней, индексированные по ELogLevel (Секция 3). */
static const char* const m_level_names[static_cast<std::size_t>(ELogLevel::Count)]
{
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

/** @brief Имя файла лога. */
constexpr const char* k_log_file_name = "agent.log";

/**
 * @brief Текущее время в формате ISO-8601 (UTC).
 * @return Строка времени.
 */
std::string timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);

    std::tm utc_time{};
    gmtime_r(&raw, &utc_time);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_time);
    return std::string(buffer);
}

} // namespace

const char* ILogger::level_name(ELogLevel level)
{
    return m_level_names[static_cast<std::size_t>(level)];
}

CLogger::CLogger(std::filesystem::path log_dir, ELogLevel min_level)
    : m_log_path(std::move(log_dir) / k_log_file_name)
    , m_min_level(min_level)
{
    std::error_code error_code;
    std::filesystem::create_directories(m_log_path.parent_path(), error_code);
}

void CLogger::log(ELogLevel level, const std::string& message)
{
    if (static_cast<int>(level) > static_cast<int>(m_min_level))
    {
        return;
    }

    std::ofstream output(m_log_path, std::ios::app);
    if (!output.is_open())
    {
        return;
    }

    output << timestamp() << " [" << level_name(level) << "] " << message << '\n';
}

const std::filesystem::path& CLogger::path() const
{
    return m_log_path;
}

} // namespace fcpp
