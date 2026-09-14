// protocol/time_check_registry.h
#pragma once

#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>

#include "../logger-common/limits.h"


class CTimeCheckRegistry
{
public:
    explicit CTimeCheckRegistry(size_t capacity = TIME_CHECK_REGISTRY_CAPACITY);

    // true  — timestamp принят, состояние обновлено
    // false — timestamp отклонён (не строго больше последнего), состояние не изменено
    bool checkAndUpdate(uint16_t id_client, uint64_t timestamp);

private:
    void touchLru(uint16_t id_client);
    void evictLru();

    struct SEntry
    {
        uint64_t                          lastTimestamp{0};
        std::list<uint16_t>::iterator     lruIt;
    };

    mutable std::mutex                                              m_mutex;
    std::unordered_map<uint16_t, SEntry>                           m_entries;
    std::list<uint16_t>                                            m_lruOrder; // front = most-recent
    size_t                                                          m_capacity;
};
