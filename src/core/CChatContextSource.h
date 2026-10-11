/**
 * @file CChatContextSource.h
 * @brief Bounded context gate for active conversation history.
 */
#ifndef FCPP_AGENT_CCHATCONTEXTSOURCE_H
#define FCPP_AGENT_CCHATCONTEXTSOURCE_H

#include "core/IContextSource.h"

namespace fcpp
{

/**
 * @brief Exposes only a bounded, prompt-local view of the supplied active chat.
 *
 * This source never queries archives and never mutates durable history.
 */
class CChatContextSource final : public IContextSource
{
public:
    explicit CChatContextSource(const std::vector<SConversationTurn>& history);

    SContextSourceResult collect_context(
        const SContextSourceRequest& request) const override;

private:
    const std::vector<SConversationTurn>& m_history;
};

} // namespace fcpp

#endif // FCPP_AGENT_CCHATCONTEXTSOURCE_H
