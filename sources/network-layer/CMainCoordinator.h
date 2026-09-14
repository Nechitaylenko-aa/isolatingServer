//
// Created by artem on 14.08.26.
//

#ifndef APP_SERVERD_CMAINCOORDINATOR_H
#define APP_SERVERD_CMAINCOORDINATOR_H

#include <cstdint>
#include <mutex>
#include <vector>

#include "pending_result.h"
#include "../include/CQueue.h"

class CWorker;

class CMainCoordinator
{
public:
    CMainCoordinator(CQueue& systemQueue, std::vector<CWorker*> workers);
    ~CMainCoordinator();

    CMainCoordinator(const CMainCoordinator&) = delete;
    CMainCoordinator& operator=(const CMainCoordinator&) = delete;

    void start();
    void stop();

    void publish(SPendingResult result); // с любого потока

private:
    void redistribute();

    CQueue&                 m_queue;
    std::vector<CWorker*>   m_workers; // индекс == worker_id

    std::mutex                   m_resultsMutex;
    std::vector<SPendingResult>  m_results;

    static constexpr uint32_t REDISTRIBUTE_PERIOD_MS = 5;
};


#endif //APP_SERVERD_CMAINCOORDINATOR_H
