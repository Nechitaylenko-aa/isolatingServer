/**
 * @file CLogger.h
 * @brief File logger writing compact structured JSONL records.
 */
#ifndef FCPP_AGENT_CLOGGER_H
#define FCPP_AGENT_CLOGGER_H

#include <filesystem>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "fcpp-agent/ILogger.h"

namespace fcpp
{
class CLogger final : public ILogger
{
public:
    CLogger(std::filesystem::path log_dir, ELogLevel min_level);
    ~CLogger() override = default;

    void log(ELogLevel level, const std::string& message) override;
    void log(ELogLevel level, const SLogEvent& event) override;
    const std::filesystem::path& path() const;
    void log_event(ELogLevel level, const std::string& event,
                   const nlohmann::json& fields = nlohmann::json::object());

private:
    std::filesystem::path m_log_path;
    ELogLevel m_min_level;
    std::mutex m_mutex;
};
} // namespace fcpp
#endif
