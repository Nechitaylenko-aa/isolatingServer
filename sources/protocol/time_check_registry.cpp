// protocol/time_check_registry.cpp
#include "time_check_registry.h"

CTimeCheckRegistry::CTimeCheckRegistry(size_t capacity)
    : m_capacity(capacity > 0 ? capacity : 1)
{}

bool CTimeCheckRegistry::checkAndUpdate(uint16_t id_client, uint64_t timestamp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_entries.find(id_client);
    if (it != m_entries.end())
    {
        if (timestamp <= it->second.lastTimestamp)
            return false;

        it->second.lastTimestamp = timestamp;
        touchLru(id_client);
        return true;
    }

    // новый клиент
    if (m_entries.size() >= m_capacity)
        evictLru();

    m_lruOrder.push_front(id_client);
    m_entries[id_client] = SEntry{timestamp, m_lruOrder.begin()};
    return true;
}

void CTimeCheckRegistry::touchLru(uint16_t id_client)
{
    auto& entry = m_entries[id_client];
    m_lruOrder.erase(entry.lruIt);
    m_lruOrder.push_front(id_client);
    entry.lruIt = m_lruOrder.begin();
}

void CTimeCheckRegistry::evictLru()
{
    if (m_lruOrder.empty())
        return;
    uint16_t victim = m_lruOrder.back();
    m_lruOrder.pop_back();
    m_entries.erase(victim);
}
