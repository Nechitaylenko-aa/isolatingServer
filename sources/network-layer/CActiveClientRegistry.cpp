//
// Created by artem on 14.08.26.
//

#include "CActiveClientRegistry.h"

bool CActiveClientRegistry::isActive(uint16_t id_client) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active.find(id_client) != m_active.end();
}

bool CActiveClientRegistry::tryAdmit(uint16_t id_client, size_t maxClients)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_active.find(id_client) != m_active.end())
        return false;
    if (m_active.size() >= maxClients)
        return false;
    m_active.insert(id_client);
    return true;
}

void CActiveClientRegistry::release(uint16_t id_client)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active.erase(id_client);
}

size_t CActiveClientRegistry::activeCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active.size();
}
