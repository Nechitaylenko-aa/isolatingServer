//
// Created by artem on 14.08.26.
//

#include "CMainCoordinator.h"
#include "CMainCoordinator.h"

#include <utility>

#include "CWorker.h"
#include "../logger-common/logger.h"

CMainCoordinator::CMainCoordinator(CQueue& systemQueue, std::vector<CWorker*> workers)
        : m_queue(systemQueue)
          , m_workers(std::move(workers))
{
}

CMainCoordinator::~CMainCoordinator()
{
    stop();
}

void CMainCoordinator::start()
{
    m_queue.submit(this, [this] { redistribute(); }, REDISTRIBUTE_PERIOD_MS);
}

void CMainCoordinator::stop()
{
    m_queue.removeTasks(this);
}

void CMainCoordinator::publish(SPendingResult result)
{
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    m_results.push_back(std::move(result));
}

void CMainCoordinator::redistribute()
{
    std::vector<SPendingResult> local;
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        local = std::move(m_results);
        m_results.clear();
    }

    for (auto& r : local)
    {
        if (r.worker_id >= m_workers.size())
        {
            CLogger::instance().error("CMainCoordinator", "неизвестный worker_id={}", r.worker_id);
            continue;
        }

        CWorkerInbox & inbox = m_workers[r.worker_id]->inbox();
        CLogger::instance().info("CMainCoordinator", "Result ready for worker id:{}", r.worker_id);
        std::lock_guard<std::mutex> lock(inbox.m_mutex);
        inbox.m_results.push_back(std::move(r));
    }
}
