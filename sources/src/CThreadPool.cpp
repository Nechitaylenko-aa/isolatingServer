#include "../include/CThreadPool.h"

struct CThreadPool::Impl
{
    std::vector<std::thread> workers;
    std::queue<Task> tasks;
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};

    explicit Impl(size_t threadCount) : workers(threadCount) {}
};



CThreadPool::CThreadPool(size_t threads)
{
    pimpl  = new Impl(threads);
    for (auto &worker : pimpl->workers)
    {
        auto p = pimpl;
        worker = std::thread([p] { CThreadPool::workLoop(p); });
    }
}

CThreadPool::~CThreadPool()
{
    shutdown();
    delete pimpl;
}

void CThreadPool::submit(void *owner, std::function<void ()> task)
{
    {
        std::lock_guard<std::mutex> lock(pimpl->mutex);
        pimpl->tasks.emplace(std::move(task), owner);
    }

    pimpl->condition.notify_one();
}

void CThreadPool::shutdown()
{
    {
        std::lock_guard lock(pimpl->mutex);
        if (pimpl->stop) return;
        pimpl->stop = true;
    }

    pimpl->condition.notify_all();

    for (auto & worker : pimpl->workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

void CThreadPool::workLoop(Impl * Pimpl)
{
    while (true)
    {
        std::function<void()> task;

        {
            std::unique_lock lock(Pimpl->mutex);
            Pimpl->condition.wait(lock, [Pimpl]
            {
                return Pimpl->stop || !Pimpl->tasks.empty();
            });

            if (Pimpl->stop)
            {
                return;
            }

            task = std::move(Pimpl->tasks.front().func);
            Pimpl->tasks.pop();

        }

        try {
            if (task && !Pimpl->stop)
                task();
        } catch (...) {
            return;
        }
    }
}

void CThreadPool::cancelOwner(void *owner)
{
    std::lock_guard lock(pimpl->mutex);

    std::queue<Task> new_tasks;
    while (!pimpl->tasks.empty())
    {
        if (pimpl->tasks.front().owner != owner)
        {
            new_tasks.push(std::move(pimpl->tasks.front()));
        }
        pimpl->tasks.pop();
    }
    pimpl->tasks = std::move(new_tasks);
}
