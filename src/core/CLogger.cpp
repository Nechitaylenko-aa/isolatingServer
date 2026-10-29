/**
 * @file CLogger.cpp
 * @brief File logger implementation.
 */
#include "core/CLogger.h"

#include <chrono>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <system_error>
#include <utility>

namespace fcpp
{
namespace
{
static const char* const m_level_names[static_cast<std::size_t>(ELogLevel::Count)]{
    "ERROR", "WARN", "INFO", "DEBUG", "TRACE"
};
constexpr const char* k_log_file_name = "agent.jsonl";

std::string timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time{};
    gmtime_r(&raw, &utc_time);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_time);
    return buffer;
}
} // namespace

const char* ILogger::level_name(ELogLevel level)
{
    const auto index = static_cast<std::size_t>(level);
    return index < static_cast<std::size_t>(ELogLevel::Count)
        ? m_level_names[index] : "UNKNOWN";
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
    log_event(level, "log", nlohmann::json{{"message", message}});
}

void CLogger::log(ELogLevel level, const SLogEvent& event)
{
    log_event(level, event.m_name, event.m_fields);
}

void CLogger::log_event(ELogLevel level, const std::string& event,
                        const nlohmann::json& fields)
{
    if (static_cast<int>(level) > static_cast<int>(m_min_level) || !fields.is_object())
        return;

    nlohmann::json record = fields;
    record["timestamp"] = timestamp();
    record["level"] = level_name(level);
    record["event"] = event;

    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream output(m_log_path, std::ios::app);
    if (output.is_open())
        output << record.dump() << '\n';
}

const std::filesystem::path& CLogger::path() const
{
    return m_log_path;
}
} // namespace fcpp
