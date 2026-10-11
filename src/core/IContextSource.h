/**
 * @file IContextSource.h
 * @brief Extension gate for independent context sources.
 *
 * Sources own their data and source-specific selection rules. The context
 * builder asks each source for a bounded contribution and then applies the
 * global model budget. This interface intentionally contains no policy for
 * archive lookup, semantic summarization, or conflict resolution.
 */

#ifndef FCPP_AGENT_ICONTEXTSOURCE_H
#define FCPP_AGENT_ICONTEXTSOURCE_H

#include <cstddef>
#include <string>
#include <vector>

#include "core/CContextManager.h"

namespace fcpp
{

/**
 * @brief Request made by the context builder to one independent source.
 *
 * m_max_tokens is the maximum contribution budget requested from this source,
 * not the model's total context window. A source must prefer a shorter useful
 * contribution over filling this allowance.
 */
struct SContextSourceRequest
{
    std::string m_task;
    std::size_t m_max_tokens = 0;
};

/**
 * @brief Bounded contribution returned by a context source gate.
 *
 * The source returns complete atomic groups only. m_estimated_tokens is
 * diagnostic; the context builder remains responsible for the final combined
 * budget check. m_complete=false means the source could not provide all
 * potentially relevant data within its own gate.
 */
struct SContextSourceResult
{
    std::vector<SContextGroup> m_groups;
    std::size_t m_estimated_tokens = 0;
    bool m_complete = true;
    std::string m_note;
};

/**
 * @brief Source-side gate used by the context builder.
 *
 * Implementations may be backed by code, the project passport, or chat. They
 * decide how to expose their own data; they do not call the LLM directly and
 * do not inspect or mutate other sources.
 */
class IContextSource
{
public:
    virtual ~IContextSource() = default;

    virtual SContextSourceResult collect_context(
        const SContextSourceRequest& request) const = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_ICONTEXTSOURCE_H
