/**
 * @file CModelLevels.h
 * @brief Shared model availability and effective endpoint identity.
 */
#ifndef FCPP_AGENT_CMODELLEVELS_H
#define FCPP_AGENT_CMODELLEVELS_H

#include <string>
#include <vector>

#include "fcpp-agent/SCascade.h"
#include "fcpp-agent/SConfig.h"

namespace fcpp
{

/** Map logical escalation level to the configured model role. */
EModelRoleKind model_role_for_level(EModelLevel level);

/** Return an identity for the effective endpoint after CModelFactory inheritance rules. */
std::string model_endpoint_signature(const SConfig& config, EModelLevel level);

/** Return available, distinct endpoints in deterministic Lite -> Main -> Remote order. */
std::vector<EModelLevel> available_model_levels(const SConfig& config);

} // namespace fcpp

#endif // FCPP_AGENT_CMODELLEVELS_H
