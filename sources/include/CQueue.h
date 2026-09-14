#ifndef REMINDER_CQUEUE_H
#define REMINDER_CQUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <atomic>
#include <set>
#include "CThreadPool.h"

struct STask
{
    std::chrono::steady_clock::time_point next_run;
    std::function<void()>   task;
    uint32_t  period_ms;
    void  *owner{nullptr};
    bool operator>(const STask& other) const
    {
        return next_run > other.next_run;
    }
};

class CQueue
{
public:
    explicit CQueue(CThreadPool * pool);
    CQueue() = delete;
    CQueue(const CQueue&) = delete;
    CQueue(CQueue &&) = delete;
    ~CQueue();

    void submit(void * owner, std::function<void()> task, uint32_t period_ms);
    void removeTasks(void * owner);
    void start();
    void stop();

private: // members
    std::priority_queue<STask, std::vector<STask>, std::greater<>> m_tasks;
    std::mutex  m_lock;
    CThreadPool * m_pool;
    std::set<void*> m_removed;
    std::atomic_bool m_start{true};
    std::atomic_bool m_shutdown{false};
    std::thread m_thread;
    std::condition_variable m_cv;
private: // methods
    void shutdown();
    void workLoop();
};


#endif //REMINDER_CQUEUE_H
