/**
 * @file CProjectProfileContextSource.h
 * @brief Bounded context gate for the project passport.
 */
#ifndef FCPP_AGENT_CPROJECTPROFILECONTEXTSOURCE_H
#define FCPP_AGENT_CPROJECTPROFILECONTEXTSOURCE_H

#include <string>

#include "core/IContextSource.h"

namespace fcpp
{

class CProjectProfileContextSource final : public IContextSource
{
public:
    explicit CProjectProfileContextSource(const std::string& profile);

    SContextSourceResult collect_context(
        const SContextSourceRequest& request) const override;

private:
    const std::string& m_profile;
};

} // namespace fcpp

#endif // FCPP_AGENT_CPROJECTPROFILECONTEXTSOURCE_H
