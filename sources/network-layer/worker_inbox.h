//
// Created by artem on 14.08.26.
//

#ifndef APP_SERVERD_WORKER_INBOX_H
#define APP_SERVERD_WORKER_INBOX_H

#include <mutex>
#include <vector>
#include "pending_result.h"

struct CWorkerInbox
{
    std::mutex                   m_mutex;
    std::vector<SPendingResult>  m_results;
};

#endif //APP_SERVERD_WORKER_INBOX_H
