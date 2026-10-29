/**
 * @file CLogger.cpp
 * @brief File logger implementation.
 */
#include "core/CLogger.h"
#include "core/CRequestOrchestrator.h"
#include "fcpp-agent/SOrchestrationTypes.h"
#include "core/CCursorContextAnalyzer.h"
#include "core/CHistorian.h"
#include "fcpp-agent/SError.h"

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

void CLogger::log(ELogLevel level, const SOrchestrationRequest& request)
{
    log_event(level, "request.started",
        {{"request_id", request.m_request_id}, {"session_id", request.m_session_id},
         {"text_length", request.m_text.size()},
         {"has_cursor", !request.m_cursor_file.empty()
             && request.m_cursor_line > 0 && request.m_cursor_column > 0},
         {"history_turns", request.m_history.size()}});
}

void CLogger::log(ELogLevel level, const SOrchestrationResult& result,
                  const std::string& request_id, const std::string& session_id,
                  long long elapsed_ms)
{
    log_event(level, "request.finished",
        {{"request_id", request_id}, {"session_id", session_id},
         {"status", request_status_name(result.m_status)}, {"route", orchestration_route_name(result.m_route)},
         {"exit_reason", exit_reason_name(result.m_exit_reason)}, {"elapsed_ms", elapsed_ms}});
}

void CLogger::log(ELogLevel level, const SCursorContextResult& result,
                  const std::string& request_id)
{
    log_event(level, "cursor.assessed",
        {{"request_id", request_id}, {"has_symbol", result.m_symbol.has_value()},
         {"llm_called", result.m_llm_called}, {"ok", result.m_ok},
         {"applicable", result.m_context_applicable}, {"escalated", result.m_escalated},
         {"error", result.m_error}});
}

void CLogger::log(ELogLevel level, const SHistoryRelevance& result,
                  const std::string& request_id, std::size_t history_turns)
{
    log_event(level, "history.assessed",
        {{"request_id", request_id}, {"history_turns", history_turns},
         {"ok", result.m_ok}, {"needed", result.m_history_needed},
         {"selected_turn_count", result.m_selected_turn_indices.size()},
         {"selected_turns", [&result]()
            {
                nlohmann::json selected = nlohmann::json::array();
                for (const std::size_t index : result.m_selected_turn_indices)
                    selected.push_back(index + 1);
                return selected;
            }()},
         {"escalated", result.m_escalated}, {"error", result.m_error}});
}

void CLogger::log(ELogLevel level, const SError& error)
{
    log_event(level, "operation.failed",
        {{"error_kind", error_kind_name(error.m_kind)}, {"message", error.m_message}});
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
