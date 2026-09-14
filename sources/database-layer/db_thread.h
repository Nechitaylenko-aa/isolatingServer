// db/db_thread.h
//
// Единственный поток доступа к БД (database.md: "Доступ к БД - строго с
// одного выделенного потока"). Кладёт задачи от любого другого потока в
// собственную FIFO-очередь, исполняет их строго последовательно на своём
// потоке; параллельно, тем же потоком, раз в 30с проверяет живость БД и
// переподключается при необходимости.
//
// Все решения по открытым вопросам ТЗ Группы 3 зафиксированы здесь и в .cpp:
//  - §0.1 (тип id организаций): сигнатура executeOrgByID(uint16_t) НЕ
//    меняется - фильтрация id > 65535 - обязанность вызывающей стороны
//    (Группа 6), не этого класса.
//  - §0.2: isHealthy() - единственная безопасная точка чтения состояния БД
//    извне DB-потока (для будущего CAdmissionControl, Группа 5).
//  - Пока !isHealthy(), задачи из очереди НЕ вычитываются и копятся -
//    m_model/m_connection не трогаются, пока БД не восстановится.
//  - stop() отбрасывает всё, что осталось в очереди, без вызова колбэков;
//    уже стартовавшая задача доигрывается до конца.
//  - executeXxx у CDatabaseModel считаются блокирующими - колбэк
//    вызывается синхронно, на DB-потоке, до возврата из executeXxx.

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "db_types_fwd.h"
#include "../include/CDatabaseModel.h"

class CDBThread
{
public:
    // Повторяют typedef'ы CDatabaseModel дословно (там они приватные -
    // переобъявлены здесь публично для удобства вызывающей стороны).
    using TQueryEquipParamCallback = std::function<void(std::vector<SEquipRow> result, EDatabaseError error)>;
    using TQueryEquipIdCallback    = std::function<void(std::vector<SEquipRow> result, EDatabaseError error)>;
    using TQueryOrgsByType          = std::function<void(std::vector<SOrgProxy> result, EDatabaseError error)>;
    using TQueryOrgsByID             = std::function<void(std::vector<SOrgProxy> result, EDatabaseError error)>;

    explicit CDBThread(SDBConnection config);
    ~CDBThread();

    CDBThread(const CDBThread&) = delete;
    CDBThread& operator=(const CDBThread&) = delete;

    void start(); // поднимает поток, устанавливает первичное соединение
    void stop();  // дожидается текущей задачи, останавливает поток, закрывает соединение

    // Безопасно вызывать из ЛЮБОГО потока (atomic read).
    bool isHealthy() const noexcept { return m_healthy.load(std::memory_order_acquire); }

    // Тонкие обёртки: кладут задачу во внутреннюю FIFO-очередь DB-потока и
    // немедленно возвращают управление. Сам вызов CDatabaseModel::executeXxx
    // и колбэк выполняются НА DB-потоке.
    //
    // ВНИМАНИЕ по executeOrgByID: сигнатура uint16_t повторяет
    // CDatabaseModel дословно (закрытая зависимость). Несоответствие с
    // protocol.md (uint32_t) закрыто в §0.1 ТЗ (вариант b): фильтрация
    // id > 65535 с логированием - обязанность вызывающей стороны
    // (Группа 6), не этого класса.
    void executeEquipQuery(std::vector<SEquipTypeKey> uniqueTypeKeys, TQueryEquipParamCallback callback);
    void executeEquipIdQuery(std::vector<uint16_t> idList, TQueryEquipIdCallback callback);
    void executeOrgByID(std::vector<uint16_t> idList, TQueryOrgsByID callback);
    void executeOrgsByType(std::vector<uint16_t> typeList, TQueryOrgsByType callback);

private:
    struct SQueuedTask
    {
        std::function<void()> run; // типостёртая обёртка над любым из 4 вызовов выше
    };

    void enqueue(std::function<void()> task); // общая логика 4 execute-методов

    void threadLoop();
    void healthCheckTick();     // simpleTest() + переподключение раз в 30с при недоступности
    void reconnect();           // не зануляет m_connection/m_model при неудаче, см. .cpp
    void runTaskSafely(const std::function<void()>& task); // try/catch, единый рубеж исключений

    SDBConnection                          m_config;
    std::unique_ptr<CAbstractConnection>    m_connection;
    std::unique_ptr<CDatabaseModel>          m_model;

    std::thread                              m_thread;
    std::atomic<bool>                        m_running{false};
    std::atomic<bool>                        m_healthy{true};

    std::mutex                                m_queueMutex;
    std::condition_variable                    m_queueCv;
    std::deque<SQueuedTask>                    m_queue;

    static constexpr uint32_t DB_RECONNECT_PERIOD_MS = 30000; // синхронно с server.md
};
