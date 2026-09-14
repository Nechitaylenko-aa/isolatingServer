//
// Created by artem on 14.08.26.
//

#ifndef APP_SERVERD_CACTIVECLIENTREGISTRY_H
#define APP_SERVERD_CACTIVECLIENTREGISTRY_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_set>

#include "../protocol/active_client_registry.h"

class CActiveClientRegistry : public IActiveClientRegistry
{
public:
    bool isActive(uint16_t id_client) const override;

    bool tryAdmit(uint16_t id_client, size_t maxClients);
    void release(uint16_t id_client);
    size_t activeCount() const;

private:
    mutable std::mutex            m_mutex;
    std::unordered_set<uint16_t>  m_active;
};


#endif //APP_SERVERD_CACTIVECLIENTREGISTRY_H
