#include "admission_control.h"

#include "../logger-common/logger.h"

CAdmissionControl::CAdmissionControl(CQueue& queue, CDBThread& dbThread)
    : m_queue(queue)
    , m_dbThread(dbThread)
{
}

CAdmissionControl::~CAdmissionControl()
{
    stop();
}

void CAdmissionControl::start()
{
    if (m_started.exchange(true, std::memory_order_acq_rel))
        return; // повторный start() - no-op

    m_queue.submit(this, [this] { checkHealth(); }, ADMISSION_HEALTH_POLL_MS);
}

void CAdmissionControl::stop()
{
    if (!m_started.exchange(false, std::memory_order_acq_rel))
        return; // stop() без start() либо повторный stop() - no-op

    m_queue.removeTasks(this);
}

void CAdmissionControl::checkHealth()
{
    const bool healthy = m_dbThread.isHealthy();
    const bool prev    = m_dbHealthy.exchange(healthy, std::memory_order_acq_rel);

    if (healthy != prev)
    {
        if (healthy)
            CLogger::instance().info("CAdmissionControl", "db health check: OK (восстановлено)");
        else
            CLogger::instance().warn("CAdmissionControl", "db health check: FAILED");
    }
}
