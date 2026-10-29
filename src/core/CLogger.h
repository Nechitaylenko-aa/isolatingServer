/**
 * @file CLogger.h
 * @brief File logger writing compact structured JSONL records.
 */
#ifndef FCPP_AGENT_CLOGGER_H
#define FCPP_AGENT_CLOGGER_H

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "fcpp-agent/ILogger.h"

namespace fcpp
{
struct SOrchestrationRequest;
struct SOrchestrationResult;
struct SCursorContextResult;
struct SHistoryRelevance;
struct SError;
class CLogger final : public ILogger
{
public:
    CLogger(std::filesystem::path log_dir, ELogLevel min_level);
    ~CLogger() override = default;

    void log(ELogLevel level, const std::string& message) override;
    void log(ELogLevel level, const SLogEvent& event) override;
    void log(ELogLevel level, const SOrchestrationRequest& request);
    void log(ELogLevel level, const SOrchestrationResult& result,
             const std::string& request_id, const std::string& session_id,
             long long elapsed_ms);
    void log(ELogLevel level, const SCursorContextResult& result,
             const std::string& request_id);
    void log(ELogLevel level, const SHistoryRelevance& result,
             const std::string& request_id, std::size_t history_turns);
    void log(ELogLevel level, const SError& error);
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
