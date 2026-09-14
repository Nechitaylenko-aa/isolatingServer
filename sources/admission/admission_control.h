#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../include/CQueue.h"      // CQueue (Группа 1)
#include "../database-layer/db_thread.h"  // CDBThread (Группа 3)
#include "../logger-common/limits.h"     // MAX_ACTIVE_CLIENTS (Группа 1/2)

// Скелет Admission Control: единственная задача на текущем этапе —
// периодически публиковать состояние здоровья БД (CDBThread::isHealthy())
// для последующего использования CAcceptor (Группа 6).
//
// CAdmissionControl НИКОГДА не обращается к CDatabaseModel/CAbstractConnection
// напрямую и не выполняет simpleTest() сам — эта логика целиком инкапсулирована
// в CDBThread (см. tz_group3). Здесь только чтение атомарного флага.
class CAdmissionControl
{
public:
    CAdmissionControl(CQueue& queue, CDBThread& dbThread);
    ~CAdmissionControl();

    CAdmissionControl(const CAdmissionControl&) = delete;
    CAdmissionControl& operator=(const CAdmissionControl&) = delete;
    CAdmissionControl(CAdmissionControl&&) = delete;
    CAdmissionControl& operator=(CAdmissionControl&&) = delete;

    // Идемпотентны: повторный start() без предшествующего stop() — no-op,
    // повторный stop() (или stop() без start()) — тоже no-op.
    void start();
    void stop();

    // Безопасно вызывать из любого потока.
    bool   isAcceptingConnections() const noexcept { return m_dbHealthy.load(std::memory_order_acquire); }
    size_t maxActiveConnections() const noexcept    { return m_maxConnections.load(std::memory_order_acquire); }

private:
    void checkHealth(); // читает m_dbThread.isHealthy(), логирует ТОЛЬКО смену состояния

    CQueue&    m_queue;
    CDBThread& m_dbThread;

    std::atomic<bool>   m_dbHealthy{true};
    std::atomic<size_t> m_maxConnections{MAX_ACTIVE_CLIENTS}; // TODO (вне этого ТЗ): динамический расчёт по метрикам CThreadPool

    std::atomic<bool> m_started{false};

    static constexpr uint32_t ADMISSION_HEALTH_POLL_MS = 1000;
};
