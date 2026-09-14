#ifndef REMINDER_CTHREADPOOL_H
#define REMINDER_CTHREADPOOL_H

#include <functional>
#include <thread>

class CThreadPool {
public:
    explicit CThreadPool(size_t threads = std::thread::hardware_concurrency());
    ~CThreadPool();

    CThreadPool(const CThreadPool&) = delete;
    CThreadPool& operator=(const CThreadPool&) = delete;

    void submit(void *owner, std::function<void()> task);
    void cancelOwner(void* owner);
    void shutdown();

private:
    struct Task
    {
        std::function<void()> func;
        void* owner;
        Task(std::function<void()> f, void* o) : func(std::move(f)), owner(o) {}
    };



    struct Impl;  // С большой буквы по конвенции
    Impl* pimpl;

    static void workLoop(Impl * Pimpl);
};


#endif //REMINDER_CTHREADPOOL_H
