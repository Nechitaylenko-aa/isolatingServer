// equip/equip_handler_registry.cpp
#include "equip_handler_registry.h"

#include "../logger-common/logger.h"

void CEquipHandlerRegistry::registerHandler(SEquipTypeKey key, std::unique_ptr<IEquipMatchHandler> handler)
{
    auto it = m_handlers.find(key);
    if (it != m_handlers.end())
    {
        CLogger::instance().warn("CEquipHandlerRegistry",
                                  "handler overwritten for natureType={} componentType={}",
                                  static_cast<int>(key.natureType), key.componentType);
    }
    m_handlers[key] = std::move(handler);
}

IEquipMatchHandler* CEquipHandlerRegistry::find(const SEquipTypeKey& key) const
{
    auto it = m_handlers.find(key);
    return it != m_handlers.end() ? it->second.get() : nullptr;
}
