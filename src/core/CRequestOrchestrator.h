/**
 * @file CRequestOrchestrator.h
 * @brief New top-level request orchestration skeleton.
 */

#ifndef FCPP_AGENT_CREQUESTORCHESTRATOR_H
#define FCPP_AGENT_CREQUESTORCHESTRATOR_H

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

#include "core/CConversationStore.h"
#include "fcpp-agent/IClang.h"
#include "fcpp-agent/SOrchestrationTypes.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{

class CLogger;
class CHistorian;
class CCursorContextAnalyzer;
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
    std::vector<std::string> m_cursor_compile_arguments;
    std::string m_context_json = "{}";
    std::vector<SConversationTurn> m_history;
    bool m_project_open = false;
};

/** @brief Service-level result of walking the orchestration tree. */
struct SOrchestrationResult
{
    bool m_ok = false;
    ERequestStatus m_status = ERequestStatus::Failed;
    EExitReason m_exit_reason = EExitReason::None;
    EOrchestrationRoute m_route = EOrchestrationRoute::NoApplicableContext;
    std::vector<ETraceEvent> m_trace;
    std::string m_message;
    std::optional<SClangSymbol> m_cursor_symbol;
};

/**
 * @brief Owns the top-level request flow. Individual stages are intentionally
 * explicit stubs until their contracts are designed and implemented.
 */
class CRequestOrchestrator
{
public:
    explicit CRequestOrchestrator(CLogger& logger, CHistorian* historian = nullptr,
                                  CCursorContextAnalyzer* cursor_analyzer = nullptr,
                                  CLlmLayer* llm = nullptr);

    SOrchestrationResult run(const SOrchestrationRequest& request) const;

private:
    enum class ERootChoice
    {
        Failure,
        CursorAndHistory,
        Cursor,
        History,
        NoContext
    };

    ERootChoice choose_root(const SOrchestrationRequest& request,
                            SOrchestrationResult& result) const;
    void generate_and_audit_default_answer(
        const SOrchestrationRequest& request, SOrchestrationResult& result) const;
    CLogger& m_logger;
    CHistorian* m_historian = nullptr;
    CCursorContextAnalyzer* m_cursor_analyzer = nullptr;
    CLlmLayer* m_llm = nullptr;
};

} // namespace fcpp

#endif // FCPP_AGENT_CREQUESTORCHESTRATOR_H
