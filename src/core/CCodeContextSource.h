/**
 * @file CCodeContextSource.h
 * @brief Bounded context gate for indexed-code evidence.
 */
#ifndef FCPP_AGENT_CCODECONTEXTSOURCE_H
#define FCPP_AGENT_CCODECONTEXTSOURCE_H

#include <vector>

#include "core/IContextSource.h"
#include "fcpp-agent/SEvidence.h"

namespace fcpp
{

class CCodeContextSource final : public IContextSource
{
public:
    explicit CCodeContextSource(const SEvidence& evidence);

    SContextSourceResult collect_context(
        const SContextSourceRequest& request) const override;

private:
    const SEvidence& m_evidence;
};

} // namespace fcpp

#endif // FCPP_AGENT_CCODECONTEXTSOURCE_H
