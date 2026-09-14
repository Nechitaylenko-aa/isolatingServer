# ТЗ — Группа 7: CServer / точка входа (финальная сборка)

Зависит от всех Групп 1–6. Ничего нового не проектирует поверх — только связывает готовые интерфейсы в рабочий процесс.


---

## 1. Состав группы

```
server/
  bootstrap.h/.cpp   — registerEquipHandlers()
  server.h/.cpp        — CServer
  main.cpp               — точка входа, argv, сигналы
```

---

## 2. `bootstrap.h/.cpp`

```cpp
#pragma once
#include "../group4/equip_handler_registry.h"

// Регистрация конкретных хендлеров подбора (CWaterFilterLightHandler и т.п.) -
// реализация алгоритмов вне зоны любого ТЗ, только сама регистрация.
void registerEquipHandlers(CEquipHandlerRegistry& registry);
```

```cpp
// bootstrap.cpp
#include "bootstrap.h"
// #include "handlers/CWaterFilterLightHandler.h" ... (по мере появления конкретных хендлеров)

void registerEquipHandlers(CEquipHandlerRegistry& registry)
{
    // registry.registerHandler({NCore::ect_water, NCore::EWB_WATER_FILTER_LIGHT},
    //     std::make_unique<CWaterFilterLightHandler>());
    // ... остальные - добавляются по мере готовности конкретных хендлеров.
    // Пусто до тех пор, пока хендлеры не написаны - CEquipQueryDispatcher корректно
    // обрабатывает отсутствие хендлера (response_amount=0 + WARN, см. ТЗ-4).
}
```

---

## 3. `CServer`

```cpp
#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "../group1/logger.h"
#include "../group1/thread_pool.h"
#include "../group1/CQueue.h"
#include "../group2/packet_cipher.h"
#include "../group2/signature_verifier.h"
#include "../group2/time_check_registry.h"
#include "../group2/request_pipeline.h"
#include "../group2/packet_serializer.h"
#include "../group3/db_config.h"
#include "../group3/db_thread.h"
#include "../group4/equip_handler_registry.h"
#include "../group4/equip_query_dispatcher.h"
#include "../group5/admission_control.h"
#include "../group6/CActiveClientRegistry.h"
#include "../group6/CMainCoordinator.h"
#include "../group6/CWorker.h"
#include "../group6/CAcceptor.h"

class CServer
{
public:
    CServer(uint16_t port, size_t workerCount = std::thread::hardware_concurrency());
    ~CServer();

    CServer(const CServer&) = delete;
    CServer& operator=(const CServer&) = delete;

    void run();  // блокирует вызывающий поток (acceptor-цикл), возвращается после stop()
    void stop(); // потокобезопасно, идемпотентно; можно звать из сигнал-хендлера

private:
    void buildPipeline();   // §3.2
    void buildWorkers();    // §3.3 - двухфазная сборка (см. критическую находку)
    void startAll();        // §3.4
    void stopAll();          // §3.5, строго обратный startAll() порядок

    uint16_t     m_port;
    size_t        m_workerCount;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    // --- Группа 1 ---
    CThreadPool   m_matchPool;
    CQueue          m_systemQueue{&m_matchPool};

    // --- Группа 2 (стейтлесс/разделяемые компоненты, по одному экземпляру на сервер) ---
    CNullPacketCipher       m_cipher;
    CNullSignatureVerifier   m_sigVerifier;
    CTimeCheckRegistry         m_timeCheck;
    std::unique_ptr<CRequestPipeline>  m_pipeline;   // строится в buildPipeline() - нужен m_activeClients
    std::unique_ptr<CPacketSerializer>  m_serializer; // отдельный экземпляр для прямого использования CWorker'ом

    // --- Группа 3 ---
    SDBConnection   m_dbConfig;
    CDBThread          m_dbThread;

    // --- Группа 4 ---
    CEquipHandlerRegistry     m_equipRegistry;
    std::unique_ptr<CEquipQueryDispatcher> m_equipDispatcher; // нужен m_dbThread+m_matchPool, строится в конструкторе

    // --- Группа 5 ---
    std::unique_ptr<CAdmissionControl> m_admissionControl; // нужен m_systemQueue+m_dbThread

    // --- Группа 6 ---
    CActiveClientRegistry                        m_activeClients;
    std::vector<std::unique_ptr<CWorker>>          m_workers;
    std::vector<std::thread>                         m_workerThreads;
    std::unique_ptr<CMainCoordinator>                  m_coordinator;
    std::unique_ptr<CAcceptor>                           m_acceptor;
};
```

### 3.1 Порядок конструирования (тело конструктора `CServer::CServer`)

```
1. CLogger::instance().init("server.log", ELogLevel::LL_INFO)   // ДО старта любых потоков, logger.md
2. m_matchPool(workerCount)
3. m_systemQueue(&m_matchPool)
4. setupDBconfig(m_dbConfig)
5. m_dbThread(m_dbConfig)                       // ещё не start() - только конструктор
6. m_equipDispatcher = make_unique<CEquipQueryDispatcher>(m_equipRegistry, m_matchPool, m_dbThread)
7. registerEquipHandlers(m_equipRegistry)        // bootstrap, ДО первого запроса, можно и после - реестр thread-safe не обязателен, наполняется до start()
8. m_admissionControl = make_unique<CAdmissionControl>(m_systemQueue, m_dbThread)
9. buildPipeline()                                // §3.2
10. buildWorkers()                                 // §3.3 - двухфазная сборка coordinator/worker
```

### 3.2 `buildPipeline()`

```cpp
void CServer::buildPipeline()
{
    m_pipeline   = std::make_unique<CRequestPipeline>(m_activeClients, m_timeCheck, m_cipher, m_sigVerifier);
    m_serializer = std::make_unique<CPacketSerializer>(m_cipher);
}
```

`m_activeClients` (Группа 6, `IActiveClientRegistry`) — единственный экземпляр на сервер, используется и внутри `CRequestPipeline` (шаг 7 валидации, read-only `isActive()`), и напрямую в каждом `CWorker` (`tryAdmit`/`release`).

### 3.3 `buildWorkers()` — разрыв цикла `CWorker` ↔ `CMainCoordinator`

```cpp
void CServer::buildWorkers()
{
    // Фаза 1: создать воркеров с coordinator=nullptr - патч §0 делает это безопасным.
    for (uint32_t i = 0; i < m_workerCount; ++i)
    {
        m_workers.push_back(std::make_unique<CWorker>(
            i, *m_pipeline, *m_serializer, m_activeClients,
            *m_admissionControl, m_dbThread, *m_equipDispatcher,
            /*coordinator=*/nullptr));
    }

    // Фаза 2: собрать raw-указатели, создать координатор.
    std::vector<CWorker*> rawWorkers;
    for (auto& w : m_workers) rawWorkers.push_back(w.get());
    m_coordinator = std::make_unique<CMainCoordinator>(m_systemQueue, rawWorkers);

    // Фаза 3: связать воркеров с координатором постфактум.
    for (auto& w : m_workers) w->setCoordinator(m_coordinator.get());

    // Фаза 4: акцептор - воркеры уже существуют, циклической зависимости нет.
    m_acceptor = std::make_unique<CAcceptor>(m_port, rawWorkers);
}
```

### 3.4 `startAll()` / `run()`

```
startAll():
    m_dbThread.start()              // соединение с БД поднимается первым
    m_admissionControl->start()
    m_systemQueue.start()            // periodic tasks: admission control health-poll, coordinator redistribute
    m_coordinator->start()
    for w in m_workers:
        m_workerThreads.push_back(std::thread([&w]{ w->run(); }))
    // acceptor НЕ стартуется здесь - см. run()

run():
    if m_running.exchange(true): return   // повторный run() - no-op
    startAll()
    m_acceptor->run()   // БЛОКИРУЕТ вызывающий поток (accept()-цикл), это и есть "acceptor-поток" из server.md
    // возврат сюда происходит только после m_acceptor->stop() (вызван из stop(), с другого потока)
    stopAll()
    m_running.store(false)
```

Итого модель потоков в точности по `server.md`: OS-поток, вызвавший `CServer::run()`, и есть выделенный acceptor-поток; `N` воркер-потоков подняты отдельно; `CDBThread` — свой поток; "главный поток"-координатор — не отдельный OS-поток, а периодическая задача на `m_systemQueue` (через `CThreadPool`), как и было решено в ТЗ Группы 6.

### 3.5 `stop()` / `stopAll()` — строго обратный порядок

```cpp
void CServer::stop()
{
    if (m_stopRequested.exchange(true)) return; // идемпотентно
    m_acceptor->stop();      // разблокирует accept() в run(), дальше stopAll() вызывается из run()
}

void CServer::stopAll()
{
    for (auto& w : m_workers) w->stop();
    for (auto& t : m_workerThreads) if (t.joinable()) t.join();

    m_coordinator->stop();
    m_systemQueue.stop();
    m_admissionControl->stop();
    m_dbThread.stop();

    CLogger::instance().shutdown(); // строго последним, logger.md
}
```

`stop()` безопасен для вызова из сигнал-обработчика (только `exchange` на atomic + `m_acceptor->stop()`, которая сама обязана быть async-signal-safe в реализации — например, установка atomic-флага, который проверяется в цикле `accept()`, а не прямой вызов blocking-примитивов из хендлера).

### Критерии готовности
- Тест сборки: `CServer` с `workerCount=1..8` конструируется без исключений на моках БД/сети (там, где применимо).
- Тест `stop()` без предшествующего `run()` — не падает, `run()` после этого сразу завершается, ничего не запуская (порядок вызовов в тесте: `stop(); run();` — `run()` видит `m_stopRequested` и завершает `startAll()`→сразу останавливает; либо, если гонка не предусмотрена явно, минимум фиксируется тестом текущее поведение `stop()` до `run()` как "no-op на будущий запуск" — уточнить по факту реализации, не блокирует остальную группу).
- Тест `stop()` из отдельного потока во время `run()` — `run()` корректно возвращается, все воркер-потоки `join()`-ены, повторный `stop()` — no-op.
- Интеграционный smoke-тест (при наличии тестовых заглушек БД): один полный цикл запрос→ответ через реальный `CServer` (не моки отдельных классов) для каждого из 4 типов запроса.

---

## 4. `main.cpp`

```cpp
#include <cstdlib>
#include <csignal>
#include <memory>
#include "server.h"

static std::unique_ptr<CServer> g_server;

extern "C" void onSignal(int)
{
    if (g_server) g_server->stop();
}

int main(int argc, char** argv)
{
    uint16_t port = 40000; // server.md: "если пусто - 40000"
    if (argc > 1)
    {
        int parsed = std::atoi(argv[1]);
        if (parsed > 0 && parsed <= 65535) port = static_cast<uint16_t>(parsed);
    }

    g_server = std::make_unique<CServer>(port);

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    g_server->run(); // блокирует до stop()
    return 0;
}
```

---

## 5. Что остаётся вне этой группы (сознательно, по `architecture.md` §9)

- Полновесный `Graceful shutdown` (сейчас — просто дожидаемся текущих соединений через `join()`, новые после `m_acceptor->stop()` не принимаются) — прежней доработки не требует, текущий `stop()` уже не роняет соединения на середине, но не пытается «мягко» ждать дольше разумного.
- Динамический `maxActiveConnections()` в `CAdmissionControl` — по-прежнему статичен (50).
- Конкретные хендлеры подбора оборудования (`equip/handlers/*`) — реализуются отдельно, `bootstrap.cpp` только их регистрирует.
- Критическая правка №3 из ТЗ-6 (имя производителя в `SEquipProxy`) — по вашему решению будет разрешена отдельно вне этого проекта, здесь не блокирует сборку.
