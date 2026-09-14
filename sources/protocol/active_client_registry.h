// protocol/active_client_registry.h
#pragma once

#include <cstdint>

class IActiveClientRegistry
{
public:
    virtual ~IActiveClientRegistry() = default;
    virtual bool isActive(uint16_t id_client) const = 0;
};

// Заглушка для юнит-тестов группы 2 — активных клиентов никогда нет
class CAlwaysInactiveRegistry final : public IActiveClientRegistry
{
public:
    bool isActive(uint16_t) const override { return false; }
};
