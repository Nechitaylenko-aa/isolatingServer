#ifndef FCPP_AGENT_CHISTORIAN_H
#define FCPP_AGENT_CHISTORIAN_H

#include <cstddef>
#include <string>
#include <vector>

#include "core/CConversationStore.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{
struct SHistoryRelevance
{
    bool m_ok = false;
    bool m_history_needed = false;
    bool m_escalated = false;
    std::string m_error;
    ELlmRole m_used_role = ELlmRole::Count;
    std::vector<std::size_t> m_relevant_turn_indices;
};

class CHistorian
{
public:
    explicit CHistorian(CLlmLayer& llm);

    SHistoryRelevance assess_relevance(
        const std::string& current_request,
        const std::vector<SConversationTurn>& history) const;

    static bool decode_relevant_indices(
        const std::string& text, std::size_t turn_count,
        std::vector<std::size_t>& indices, std::string& error);

private:
    CLlmLayer& m_llm;
};
} // namespace fcpp

#endif
