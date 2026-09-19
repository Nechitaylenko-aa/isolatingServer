//
// Created by artem on 04.04.24.
//

#include "../include/CInfoBus.h"
//#include "CEquipmentFactory.h"
#include "../include/NComponent.h"


namespace NCore
{
    CInfoBus::CInfoBus(NCore::CCell *owner) {}

    CInfoBus::~CInfoBus() = default;

    void CInfoBus::addRequest(SEquipmentRequest&& req)
    {
        auto it = std::find_if(m_pendingRequests.begin(),
                               m_pendingRequests.end(),
                               [&](const SEquipmentRequest &item)
        {
            return item.sender == req.sender;
        });

        if (it != m_pendingRequests.end())
        {
            fprintf(stderr, "the same component trying to send request\n");
            return;
        }

        m_pendingRequests.push_back(std::move(req));
    }

    std::vector<SEquipmentRequest> CInfoBus::extractPendingRequests()
    {
        std::vector<SEquipmentRequest> result = std::move(m_pendingRequests);
        m_pendingRequests.clear();
        return result;
    }

    bool CInfoBus::is_requestEmpty() const
    {
        return m_pendingRequests.empty();
    }

    void CInfoBus::addResponse(const SEquipmentResponse &response)
    {
        m_responses.push_back(response);
    }

    void CInfoBus::processResponses()
    {
        // process responses
        for (auto &resp: m_responses)
        {
            NCore::NComponent *component = resp.target;

            if (resp.rawProxyData.empty())
            {
                continue;
            }

            // deserialization
            //nlohmann::json j(resp.content.front().equipmentJson);

        }
        m_responses.clear();
    }

    void CInfoBus::addRequirement(SReagentRequirement &&requirement)
    {

    }

    bool CInfoBus::is_requirementEmpty() const
    {
        return false;
    }

    std::vector<SReagentRequirement> CInfoBus::extractPendingRequirements()
    {
        return std::vector<SReagentRequirement>();
    }
}
