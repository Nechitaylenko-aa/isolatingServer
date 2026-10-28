#ifndef FCPP_AGENT_CHISTORIAN_H
#define FCPP_AGENT_CHISTORIAN_H
#include <string>
#include <vector>
#include "core/CConversationStore.h"
#include "llm/CLlmLayer.h"
namespace fcpp {
struct SHistoryRelevance {
    bool m_ok=false; bool m_history_needed=false; bool m_escalated=false;
    std::string m_error; ELlmRole m_used_role=ELlmRole::Count;
};
class CHistorian {
public:
    explicit CHistorian(CLlmLayer& llm);
    SHistoryRelevance assess_relevance(const std::string& current_request,
                                       const std::vector<SConversationTurn>& history) const;
private: CLlmLayer& m_llm;
};
}
#endif
