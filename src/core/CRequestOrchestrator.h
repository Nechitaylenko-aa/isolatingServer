/**
 * @file CRequestOrchestrator.h
 * @brief New top-level request orchestration skeleton.
 */

#ifndef FCPP_AGENT_CREQUESTORCHESTRATOR_H
#define FCPP_AGENT_CREQUESTORCHESTRATOR_H

#include <cstdint>
#include <string>
#include <vector>

#include "core/CConversationStore.h"

namespace fcpp
{

class CLogger;
class CProjectManager;

/** @brief Normalized input passed from the client-facing API facade. */
struct SOrchestrationRequest
{
    std::string m_request_id;
    std::string m_text;
    std::string m_session_id;
    std::string m_cursor_file;
    std::uint32_t m_cursor_line = 0;
    std::uint32_t m_cursor_column = 0;
    std::string m_context_json = "{}";
    std::vector<SConversationTurn> m_history;
    bool m_project_open = false;
};

/** @brief Service-level result of walking the orchestration tree. */
struct SOrchestrationResult
{
    bool m_ok = false;
    std::string m_status;
    std::string m_exit_reason;
    std::string m_route;
    std::vector<std::string> m_trace;
    std::string m_message;
};

/**
 * @brief Owns the top-level request flow. Individual stages are intentionally
 * explicit stubs until their contracts are designed and implemented.
 */
class CRequestOrchestrator
{
public:
    explicit CRequestOrchestrator(CLogger& logger);

    SOrchestrationResult run(const SOrchestrationRequest& request) const;

private:
    CLogger& m_logger;
};

} // namespace fcpp

#endif // FCPP_AGENT_CREQUESTORCHESTRATOR_H
