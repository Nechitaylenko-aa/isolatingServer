/**
 * @file SLogEvent.h
 * @brief Typed, structured event accepted by the logging infrastructure.
 */
#ifndef FCPP_AGENT_SLOGEVENT_H
#define FCPP_AGENT_SLOGEVENT_H
#include <cstddef>
#include <string>
#include <utility>
#include <nlohmann/json.hpp>
#include "fcpp-agent/SOrchestrationTypes.h"
namespace fcpp
{
/** @brief A semantic event; formatting and serialization belong to the logger. */
struct SLogEvent
{
    std::string m_name;
    nlohmann::json m_fields = nlohmann::json::object();
    SLogEvent(std::string name, nlohmann::json fields = nlohmann::json::object())
        : m_name(std::move(name)), m_fields(std::move(fields)) {}
    static SLogEvent request_started(const std::string&, const std::string&,
                                     std::size_t, bool, std::size_t);
    static SLogEvent request_finished(const std::string&, const std::string&,
                                      const std::string&, const std::string&,
                                      const std::string&, long long);
    static SLogEvent cursor_assessed(const std::string&, bool, bool, bool, bool, bool);
    static SLogEvent history_assessed(const std::string&, std::size_t, bool, bool, bool);
    static SLogEvent route_selected(const std::string& request_id, EOrchestrationRoute route);
    static SLogEvent operation_failed(const std::string&, const std::string&);
    static SLogEvent llm_escalated(const std::string&, const std::string&, const std::string&);
};
}
#endif
