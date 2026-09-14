// db/db_thread.cpp

#include "db_thread.h"

#include <chrono>

#include "db_config.h"
#include "../logger-common/logger.h"

namespace
{
constexpr const char* kComponent = "CDBThread";
} // namespace

CDBThread::CDBThread(SDBConnection config)
    : m_config(std::move(config))
{
}

CDBThread::~CDBThread()
{
    stop();
}

void CDBThread::start()
{
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true))
        return; // повторный start() - no-op, идемпотентно

    m_thread = std::thread(&CDBThread::threadLoop, this);
}

void CDBThread::stop()
{
    if (!m_running.exchange(false))
        return; // уже остановлен / не запускался

    m_queueCv.notify_all();

    if (m_thread.joinable())
        m_thread.join();

    // threadLoop сам отбрасывает остаток очереди при выходе (см. ниже),
    // но подстраховываемся и здесь на случай, если поток не был запущен
    // (start() не вызывался) и очередь успела накопиться до stop().
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_queue.clear();
}

void CDBThread::enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.push_back(SQueuedTask{std::move(task)});
    }
    m_queueCv.notify_one();
}

void CDBThread::executeEquipQuery(std::vector<SEquipTypeKey> uniqueTypeKeys, TQueryEquipParamCallback callback)
{
    enqueue([this, keys = std::move(uniqueTypeKeys), cb = std::move(callback)]() mutable {
        m_model->executeEquipQuery(std::move(keys), std::move(cb));
    });
}

void CDBThread::executeEquipIdQuery(std::vector<uint16_t> idList, TQueryEquipIdCallback callback)
{
    enqueue([this, ids = std::move(idList), cb = std::move(callback)]() mutable {
        m_model->executeEquipIdQuery(std::move(ids), std::move(cb));
    });
}

void CDBThread::executeOrgByID(std::vector<uint16_t> idList, TQueryOrgsByID callback)
{
    enqueue([this, ids = std::move(idList), cb = std::move(callback)]() mutable {
        m_model->executeOrgByID(std::move(ids), std::move(cb));
    });
}

void CDBThread::executeOrgsByType(std::vector<uint16_t> typeList, TQueryOrgsByType callback)
{
    enqueue([this, types = std::move(typeList), cb = std::move(callback)]() mutable {
        m_model->executeOrgsByType(std::move(types), std::move(cb));
    });
}

void CDBThread::runTaskSafely(const std::function<void()>& task)
{
    try
    {
        task();
    }
    catch (const std::exception& e)
    {
        CLogger::instance().error(kComponent, "unhandled exception in DB task: {}", e.what());
        // Если исключение вылетело ДО того, как колбэк вызывающей стороны
        // был вызван, клиент останется без ответа - это тот же осознанный
        // fallback, что и в CThreadPool (architecture.md §6). Основная
        // гарантия ответа - на задаче, формирующей запрос (Группа 4/6), а
        // не на этой защитной сетке.
    }
    catch (...)
    {
        CLogger::instance().error(kComponent, "unhandled non-std exception in DB task");
    }
}

void CDBThread::reconnect()
{
    // Решение (см. tz_group3_database_layer.md, реконнект): при неудаче
    // m_connection/m_model НЕ зануляются - остаются прежними объектами.
    // Гонок нет: m_model читается/пишется только внутри threadLoop /
    // healthCheckTick / reconnect(), все три работают строго
    // последовательно на одном (DB-) потоке.
    std::unique_ptr<CAbstractConnection> newConnection(createConnection(m_config));
    if (!newConnection)
    {
        CLogger::instance().warn(kComponent, "reconnect: createConnection returned null, keeping previous connection");
        return;
    }

    auto newModel = std::make_unique<CDatabaseModel>(newConnection.get());

    // Новое соединение/модель заменяют старые только целиком одной парой -
    // не оставляем model, указывающую на уже уничтоженный connection.
    m_connection = std::move(newConnection);
    m_model      = std::move(newModel);
}

void CDBThread::healthCheckTick()
{
    const bool ok = m_model->simpleTest();
    const bool wasHealthy = m_healthy.load(std::memory_order_relaxed);

    if (ok != wasHealthy)
    {
        if (ok)
            CLogger::instance().info(kComponent, "db health check: OK (восстановлено)");
        else
            CLogger::instance().warn(kComponent, "db health check: FAILED");
    }

    m_healthy.store(ok, std::memory_order_release);

    if (!ok)
        reconnect(); // попытка - не чаще раза в DB_RECONNECT_PERIOD_MS (см. threadLoop)
}

void CDBThread::threadLoop()
{
    m_connection.reset(createConnection(m_config));
    m_model = std::make_unique<CDatabaseModel>(m_connection.get());

    m_healthy.store(m_model->simpleTest(), std::memory_order_release);

    auto lastHealthCheck = std::chrono::steady_clock::now();
    const auto period     = std::chrono::milliseconds(DB_RECONNECT_PERIOD_MS);

    while (m_running.load(std::memory_order_acquire))
    {
        const auto now         = std::chrono::steady_clock::now();
        const auto deadline     = lastHealthCheck + period;

        if (m_healthy.load(std::memory_order_acquire))
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            if (m_queue.empty())
            {
                // Ждём либо новую задачу, либо истечения таймаута до
                // следующего health-check тика (что раньше).
                if (now < deadline)
                    m_queueCv.wait_until(lock, deadline, [this] {
                        return !m_queue.empty() || !m_running.load(std::memory_order_acquire);
                    });
            }

            if (!m_queue.empty() && m_running.load(std::memory_order_acquire))
            {
                SQueuedTask task = std::move(m_queue.front());
                m_queue.pop_front();
                lock.unlock();

                runTaskSafely(task.run);
            }
        }
        else
        {
            // Пока БД нездорова - к m_model/m_connection не прикасаемся,
            // задачи из очереди НЕ вычитываются и копятся молча. Просто
            // ждём наступления следующего health-check тика.
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (now < deadline)
                m_queueCv.wait_until(lock, deadline, [this] {
                    return !m_running.load(std::memory_order_acquire);
                });
        }

        if (!m_running.load(std::memory_order_acquire))
            break;

        if (std::chrono::steady_clock::now() - lastHealthCheck >= period)
        {
            healthCheckTick();
            lastHealthCheck = std::chrono::steady_clock::now();
        }
    }

    // stop() вызван: всё, что осталось в очереди на этот момент,
    // отбрасывается без вызова колбэков (осознанное решение, см. ТЗ).
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.clear();
    }

    m_model.reset();
    m_connection.reset();
}
