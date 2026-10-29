#include "fcpp-agent/SLogEvent.h"

namespace fcpp
{

SLogEvent SLogEvent::request_started(const std::string& request_id,
                                     const std::string& session_id,
                                     std::size_t text_length,
                                     bool has_cursor,
                                     std::size_t history_turns)
{
    return {"request.started", {{"request_id", request_id}, {"session_id", session_id},
        {"text_length", text_length}, {"has_cursor", has_cursor},
        {"history_turns", history_turns}}};
}

SLogEvent SLogEvent::request_finished(const std::string& request_id,
                                      const std::string& session_id,
                                      const std::string& status,
                                      const std::string& route,
                                      const std::string& exit_reason,
                                      long long elapsed_ms)
{
    return {"request.finished", {{"request_id", request_id}, {"session_id", session_id},
        {"status", status}, {"route", route}, {"exit_reason", exit_reason},
        {"elapsed_ms", elapsed_ms}}};
}

SLogEvent SLogEvent::cursor_assessed(const std::string& request_id,
                                     bool has_symbol,
                                     bool llm_called,
                                     bool ok,
                                     bool applicable,
                                     bool escalated)
{
    return {"cursor.assessed", {{"request_id", request_id}, {"has_symbol", has_symbol},
        {"llm_called", llm_called}, {"ok", ok}, {"applicable", applicable},
        {"escalated", escalated}}};
}

SLogEvent SLogEvent::history_assessed(const std::string& request_id,
                                      std::size_t history_turns,
                                      bool ok,
                                      bool needed,
                                      bool escalated)
{
    return {"history.assessed", {{"request_id", request_id},
        {"history_turns", history_turns}, {"ok", ok}, {"needed", needed},
        {"escalated", escalated}}};
}

SLogEvent SLogEvent::route_selected(const std::string& request_id,
                                      const std::string& route)
{
    return {"request.route", {{"request_id", request_id}, {"route", route}}};
}

SLogEvent SLogEvent::operation_failed(const std::string& component,
                                      const std::string& reason)
{
    return {"operation.failed", {{"component", component}, {"reason", reason}}};
}

SLogEvent SLogEvent::llm_escalated(const std::string& from_role,
                                   const std::string& to_role,
                                   const std::string& reason)
{
    return {"llm.escalated", {{"from_role", from_role}, {"to_role", to_role},
        {"reason", reason}}};
}

} // namespace fcpp
