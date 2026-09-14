#include "../include/CQueue.h"

CQueue::CQueue(CThreadPool *pool)
        : m_pool(pool)
{
    m_thread = std::thread([this](){this->workLoop();});
}

CQueue::~CQueue()
{
    shutdown();
}

void CQueue::submit(void *owner, std::function<void()> task, uint32_t period_ms)
{
    STask item;
    item.task = std::move(task);
    item.period_ms = period_ms;
    item.owner = owner;
    item.next_run = std::chrono::steady_clock::now() + std::chrono::milliseconds(period_ms);

    {
        std::lock_guard lock(m_lock);
        m_tasks.push(item);
        auto it = m_removed.find(owner);
        if (it != m_removed.end())
        {
            m_removed.erase(it);
        }
    }

    m_cv.notify_one();
}

void CQueue::removeTasks(void *owner)
{
    std::lock_guard lock(m_lock);
    m_removed.emplace(owner);
}

void CQueue::start()
{
    m_start = true;
}

void CQueue::stop()
{
    m_start = false;
}

void CQueue::workLoop()
{
    while (!m_shutdown)
    {
        std::unique_lock lock(m_lock);

        m_cv.wait(lock, [this] {
            return !m_tasks.empty() || m_shutdown || !m_start;
        });

        if (m_shutdown) break;
        if (!m_start) continue;

        auto now = std::chrono::steady_clock::now();
        STask topTask = m_tasks.top();

        if (m_removed.count(topTask.owner))
        {
            m_tasks.pop();
            // m_removed.erase(topTask.owner);
            continue;
        }

        if (now >= topTask.next_run)
        {
            m_tasks.pop();

            STask next = topTask;
            next.next_run = topTask.next_run + std::chrono::milliseconds(topTask.period_ms);
            m_tasks.push(next);

            lock.unlock();
            m_pool->submit(topTask.owner, topTask.task);
        }
        else
        {
            m_cv.wait_until(lock, topTask.next_run);
        }
    }
}

void CQueue::shutdown()
{
    m_shutdown = true;
    m_start = false;

    m_cv.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}
