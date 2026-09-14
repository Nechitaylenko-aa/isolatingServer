
1. Зависание соединения при недоступной БД. db_thread.h: "Пока !isHealthy(), задачи из очереди НЕ вычитываются и копятся". Если воркер отправит запрос в CDBThread/CEquipQueryDispatcher во время простоя БД — колбэк может не прийти вообще, а TCP-соединение = ровно 1 запрос-ответ, таймаута нигде не предусмотрено → соединение виснет навсегда. Резолюция: воркер проверяет CAdmissionControl::isAcceptingConnections() перед ЛЮБЫМ обращением к CDBThread/диспетчеру; при false — сразу ERS_DATABASE_QUERY_ERROR, БД вообще не трогается.
2. Черновик CAcceptor (architecture.md) противоречит server.md. Черновик гасит accept() по isAcceptingConnections() (здоровье БД), а server.md (раздел Database) прямо требует: при недоступности БД "сервер продолжает принимать соединения... отвечает ERS_DATABASE_QUERY_ERROR на все запросы". Резолюция: CAcceptor вообще не зависит от CAdmissionControl — приём никогда не блокируется здоровьем БД; всё закрывается пунктом 1 на уровне запроса.
3. Дыра в данных: имя производителя. SEquipProxy (протокол, ответ на EPT_EQIP_ID) требует `char manufacturer[30]` — имя. SEquipRow (database.md) даёт только id_manufacturer (id), имени производителя нет нигде в текущих ТЗ. Резолюция (временная): поле отдаётся пустым, WARN в лог один раз при первом столкновении;


# ТЗ — Группа 6: Сетевой слой

Статус: шестая группа. Зависит от всех предыдущих: Группа 1 (`CLogger`, `CQueue`, `CThreadPool`), Группа 2 (`CRequestPipeline`, `CPacketSerializer`, `IActiveClientRegistry`, `TParsedPayload`), Группа 3 (`CDBThread`), Группа 4 (`CEquipQueryDispatcher`), Группа 5 (`CAdmissionControl`).

Критические несостыковки и их резолюции — см. врезку перед этим документом (в начале ответа), они являются неотъемлемой частью этого ТЗ и здесь не повторяются текстом, только в виде готовых решений в алгоритмах ниже.

Источники: `server.md`, `architecture.md` §2–3, `protocol.md`, плюс все реализованные интерфейсы Групп 1–5.

---

## 0. Состав группы

```
net/
  active_client_registry_impl.h/.cpp — CActiveClientRegistry (реальная реализация IActiveClientRegistry)
  pending_result.h                    — SPendingResult
  worker_inbox.h                       — CWorkerInbox
  main_coordinator.h/.cpp               — CMainCoordinator
  worker.h/.cpp                          — CWorker
  acceptor.h/.cpp                         — CAcceptor
```

Реализация конкретного event-loop (`kqueue`/`io_uring`) — OS-специфичный код внутри `CWorker`/`CAcceptor`, экранируется `#ifdef Linux` / `#ifdef FreeBSD` (`server.md`). В этом ТЗ описывается интерфейс и алгоритм на уровне логики; сами системные вызовы — на усмотрение реализации.

---

## 1. `CActiveClientRegistry` — реальный `IActiveClientRegistry` + резервирование слота

```cpp
#pragma once
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include "active_client_registry.h" // IActiveClientRegistry (Группа 2)

class CActiveClientRegistry final : public IActiveClientRegistry
{
public:
    bool isActive(uint16_t id_client) const override; // read-only, контракт Группы 2 не меняется

    // Атомарно: если id_client уже активен ИЛИ activeCount() >= maxClients -> false, без изменений.
    // Иначе -> добавляет id_client, возвращает true.
    // Закрывает гонку между CHeaderValidator::validate() (шаг 7, только чтение) и фактическим
    // резервированием слота, и одновременно реализует лимит MAX_ACTIVE_CLIENTS=50
    // ("Проверяется вместе с проверкой id_client уже активен", architecture.md).
    bool tryAdmit(uint16_t id_client, size_t maxClients);

    // Вызывается РОВНО ОДИН РАЗ на каждое успешное tryAdmit(), по завершении обработки
    // соединения (ответ отправлен либо разрыв на любом этапе после tryAdmit).
    void release(uint16_t id_client);

    size_t activeCount() const;

private:
    mutable std::mutex            m_mutex;
    std::unordered_set<uint16_t>  m_active;
};
```

Единственный экземпляр на весь сервер (создаётся в Группе 7, передаётся всем воркерам по ссылке) — реестр должен быть общим для всех воркеров, т.к. один и тот же `id_client` теоретически может попасть на любой из них.

### Критерии готовности
- Тест: `tryAdmit` дважды одним и тем же `id_client` — второй раз `false`, `activeCount()` не меняется.
- Тест: `tryAdmit` при `activeCount() == maxClients` новым `id_client` — `false`.
- Тест: `release` затем повторный `tryAdmit` тем же `id_client` — `true`.
- Конкурентный тест: N потоков одновременно `tryAdmit` с одним и тем же `id_client` — ровно один успех.

---

## 2. `SPendingResult` / `CWorkerInbox`

```cpp
#pragma once
#include <cstdint>
#include <vector>

struct SPendingResult
{
    uint32_t              worker_id{0};
    uint32_t              request_id{0}; // локальный монотонный счётчик воркера, см. architecture.md §3
    std::vector<uint8_t>  responseBytes; // уже полностью сериализован (CPacketSerializer), готов к записи в сокет
};

struct CWorkerInbox
{
    std::mutex                    m_mutex;
    std::vector<SPendingResult>  m_results;
};
```

`responseBytes` собирается ДО попадания в `SPendingResult` — сериализация (`CPacketSerializer`) не имеет состояния, привязанного к конкретному воркеру, поэтому безопасно выполняется на том потоке, где готов результат (DB-поток или поток `CThreadPool`), не на потоке воркера — сама запись в сокет остаётся строго за воркером-владельцем соединения (`server.md`: "Запись в сокет всегда выполняется тем же воркер-потоком").

---

## 3. `CMainCoordinator`

```cpp
#pragma once
#include <mutex>
#include <vector>
#include "pending_result.h"
#include "worker_inbox.h"
#include "queue.h" // CQueue

class CWorker; // fwd

class CMainCoordinator
{
public:
    CMainCoordinator(CQueue& systemQueue, std::vector<CWorker*> workers);
    ~CMainCoordinator();

    void start(); // systemQueue.submit(this, [this]{ redistribute(); }, REDISTRIBUTE_PERIOD_MS)
    void stop();  // systemQueue.removeTasks(this)

    // Вызывается с ЛЮБОГО потока (DB-поток, поток CThreadPool). Потокобезопасно.
    void publish(SPendingResult result);

private:
    void redistribute(); // забирает всё из g_results под мьютексом, раскладывает по CWorker::inbox() по worker_id

    CQueue&                 m_queue;
    std::vector<CWorker*>   m_workers; // индекс в векторе == worker_id

    std::mutex                    m_resultsMutex;
    std::vector<SPendingResult>  m_results; // общий буфер (server.md: "g_results")

    static constexpr uint32_t REDISTRIBUTE_PERIOD_MS = 5; // тот же порядок, что и WORKER_INBOX_POLL_MS; TODO при тюнинге
};
```

`redistribute()`: под `m_resultsMutex` — `std::move(m_results)` в локальную переменную, `m_results.clear()`, отпустить мьютекс; затем для каждого элемента — `m_workers[result.worker_id]->inbox().m_mutex` lock, `push_back`, unlock. Схема — дословно по `server.md` ("DB-поток → главный поток (один общий буфер) → главный поток раскладывает по persoediv-воркерным inbox'ам").

### Критерии готовности
- Тест: `publish()` из нескольких потоков одновременно — все результаты попадают в `m_results` без потерь (сверка по количеству/содержимому после `redistribute()`).
- Тест: `redistribute()` корректно направляет каждый результат в `inbox()` воркера с соответствующим `worker_id`, не путает воркеров.

---

## 4. `CWorker`

```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "worker_inbox.h"
#include "request_pipeline.h"     // CRequestPipeline (Группа 2)
#include "packet_serializer.h"     // CPacketSerializer (Группа 2)
#include "active_client_registry_impl.h"
#include "admission_control.h"      // CAdmissionControl (Группа 5)
#include "db_thread.h"                // CDBThread (Группа 3)
#include "equip_query_dispatcher.h"    // CEquipQueryDispatcher (Группа 4)
#include "limits.h"                     // MAX_ACTIVE_CLIENTS

class CWorker
{
public:
    CWorker(uint32_t workerId,
             CRequestPipeline&           pipeline,
             CPacketSerializer&           serializer,
             CActiveClientRegistry&        activeClients,
             CAdmissionControl&             admission,
             CDBThread&                      dbThread,
             CEquipQueryDispatcher&           equipDispatcher);

    void run();   // event-loop потока (kqueue/io_uring, OS-специфично)
    void stop();

    // Вызывается из CAcceptor (другой поток) - потокобезопасная передача fd воркеру.
    void assignConnection(int fd);

    CWorkerInbox& inbox() { return m_inbox; }
    uint32_t      id() const noexcept { return m_workerId; }
    size_t         activeConnections() const noexcept { return m_activeConnCount.load(); }

private:
    struct SConnectionCtx
    {
        int       fd{-1};
        uint16_t  id_client{0};
        bool      admitted{false}; // true, если tryAdmit() прошёл успешно - нужно release() при закрытии
    };

    // --- обработка нового fd (из очереди назначений, см. §4.2) ---
    void onNewConnection(int fd);

    // --- полное сообщение прочитано с сокета (rawData - весь буфер, включая encrypted_flag) ---
    void onRequestReady(SConnectionCtx& ctx, std::vector<uint8_t> rawData);

    // --- маршрутизация распарсенного payload в БД/диспетчер ---
    void dispatchToBackend(SConnectionCtx& ctx, const SClientPacket& header, TParsedPayload payload);

    // --- периодический опрос своего inbox (таймер event-loop, WORKER_INBOX_POLL_MS) ---
    void pollInbox();

    void writeAndClose(SConnectionCtx& ctx, std::vector<uint8_t> responseBytes);
    void closeConnection(SConnectionCtx& ctx); // без ответа; всегда release() если admitted

    uint32_t                          m_workerId;
    CRequestPipeline&                   m_pipeline;
    CPacketSerializer&                    m_serializer;
    CActiveClientRegistry&                  m_activeClients;
    CAdmissionControl&                        m_admission;
    CDBThread&                                  m_dbThread;
    CEquipQueryDispatcher&                        m_equipDispatcher;

    CWorkerInbox                                    m_inbox;
    std::atomic<uint32_t>                             m_nextRequestId{0}; // per-worker монотонный счётчик, architecture.md §3
    std::atomic<size_t>                                m_activeConnCount{0};

    // request_id -> контекст соединения; читается/пишется ТОЛЬКО из потока этого воркера
    // (и запись при отправке запроса в БД, и чтение при разборе inbox происходят на одном потоке) - без мьютекса.
    std::unordered_map<uint32_t, SConnectionCtx>          m_pendingByRequestId;

    // fd, переданные CAcceptor'ом, но ещё не подхваченные event-loop'ом - под мьютексом,
    // т.к. assignConnection() зовётся с чужого потока.
    std::mutex                                              m_newConnMutex;
    std::vector<int>                                          m_newConnFds;

    static constexpr uint32_t WORKER_INBOX_POLL_MS = 5; // architecture.md §3, TODO при тюнинге
};
```

### 4.1 Жизненный цикл соединения — полный алгоритм

```
onNewConnection(fd):
    ctx = SConnectionCtx{fd}
    читаем сообщение целиком (учитывая лимиты 500кБ/200кБ - см. protocol/limits.h; при
        превышении - закрыть без ответа ДО полного вычитывания, не дожидаясь остатка)
    onRequestReady(ctx, rawData)

onRequestReady(ctx, rawData):
    result = m_pipeline.process(rawData.data(), rawData.size())

    switch result.action:
        DISCONNECT:
            closeConnection(ctx)
            return
        RESPOND:
            writeAndClose(ctx, result.responseBytes) // ctx.admitted == false, release() не вызывается
            return
        PROCEED:
            id_client = result.header.id_client
            if !m_activeClients.tryAdmit(id_client, MAX_ACTIVE_CLIENTS):
                closeConnection(ctx) // ctx.admitted остаётся false - лимит/дубликат
                return
            ctx.id_client = id_client
            ctx.admitted   = true

            // КРИТИЧЕСКАЯ ПРАВКА №1 (см. врезку) - гейт по здоровью БД ДО обращения к CDBThread/диспетчеру
            if !m_admission.isAcceptingConnections():
                resp = m_serializer.serializeError(result.header, ERS_DATABASE_QUERY_ERROR, /*encryptResponse=*/wasEncrypted(result.header))
                writeAndClose(ctx, resp) // release() вызывается внутри writeAndClose/closeConnection
                return

            dispatchToBackend(ctx, result.header, result.payload)

dispatchToBackend(ctx, header, payload):
    requestId = m_nextRequestId.fetch_add(1)
    m_pendingByRequestId[requestId] = ctx   // сохранить контекст ДО ухода в БД

    encryptResp = wasEncrypted(header)

    std::visit(overloaded{
        [&](const std::vector<SComponentReq>& components) {
            m_equipDispatcher.dispatch(components,
                [this, workerId = m_workerId, requestId, header, encryptResp](EResponseStatus status, std::vector<SEquipReqRespBlock> blocks) {
                    auto bytes = (status == ERS_SUCCESS)
                        ? m_serializer.serializeEquipReqResponse(header, blocks, encryptResp)
                        : m_serializer.serializeError(header, status, encryptResp);
                    m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                });
        },
        [&](const SEquipIdRequest& req) {
            m_dbThread.executeEquipIdQuery(req.ids,
                [this, workerId = m_workerId, requestId, header, encryptResp](std::vector<SEquipRow> rows, EDatabaseError error) {
                    std::vector<uint8_t> bytes;
                    if (error != DATABSE_OK) {
                        bytes = m_serializer.serializeError(header, ERS_DATABASE_QUERY_ERROR, encryptResp);
                    } else {
                        auto items = buildEquipProxyItems(rows); // см. §4.3 - КРИТИЧЕСКАЯ ПРАВКА №3 внутри
                        bytes = m_serializer.serializeEquipIdResponse(header, items, encryptResp);
                    }
                    m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                });
        },
        [&](const SOrgTypeRequest& req) {
            m_dbThread.executeOrgsByType(req.types,
                [this, workerId = m_workerId, requestId, header, encryptResp](std::vector<SOrgProxy> orgs, EDatabaseError error) {
                    auto bytes = (error == DATABSE_OK)
                        ? m_serializer.serializeOrgsResponse(header, orgs, encryptResp)
                        : m_serializer.serializeError(header, ERS_DATABASE_QUERY_ERROR, encryptResp);
                    m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                });
        },
        [&](const SOrgIdRequest& req) {
            auto [filtered, droppedCount] = filterOrgIdsToUint16(req.ids); // §4.4 - обязанность Группы 6 по §0.1 ТЗ-3
            if (droppedCount > 0)
                CLogger::instance().warn("CWorker", "EPT_ORGS_ID: {} id отброшено (>65535)", droppedCount);
            m_dbThread.executeOrgByID(filtered,
                [this, workerId = m_workerId, requestId, header, encryptResp](std::vector<SOrgProxy> orgs, EDatabaseError error) {
                    auto bytes = (error == DATABSE_OK)
                        ? m_serializer.serializeOrgsResponse(header, orgs, encryptResp)
                        : m_serializer.serializeError(header, ERS_DATABASE_QUERY_ERROR, encryptResp);
                    m_coordinator->publish(SPendingResult{workerId, requestId, std::move(bytes)});
                });
        },
    }, payload)
```

### 4.2 `pollInbox()` — вызывается таймером event-loop'а раз в `WORKER_INBOX_POLL_MS`

```
results = []
{ lock m_inbox.m_mutex; results = move(m_inbox.m_results); m_inbox.m_results.clear() }

for r in results:
    it = m_pendingByRequestId.find(r.request_id)
    if it == end: // не должно происходить в норме - лог ERROR, пропустить
        CLogger::instance().error("CWorker", "inbox: неизвестный request_id={}", r.request_id)
        continue
    ctx = it->second
    m_pendingByRequestId.erase(it)
    writeAndClose(ctx, r.responseBytes)
```

`writeAndClose`/`closeConnection` в конце ВСЕГДА вызывают `m_activeClients.release(ctx.id_client)`, если `ctx.admitted == true`, и уменьшают `m_activeConnCount`; закрывают fd безусловно (`server.md`: "сразу disconnect и удалить контекст клиента" — соединение не переживает ответ в любом случае).

### 4.3 `buildEquipProxyItems` (КРИТИЧЕСКАЯ ПРАВКА №3 применяется здесь)

```cpp
std::vector<SEquipProxyRespItem> CWorker::buildEquipProxyItems(const std::vector<SEquipRow>& rows)
{
    auto grouped = groupEquipRowsByType(rows); // Группа 3, игнорируем разбивку по SEquipTypeKey - flatten
    std::vector<SEquipProxyRespItem> items;
    for (auto& [key, list] : grouped)
        for (auto& g : list)
        {
            SEquipProxyRespItem item;
            item.meta.id              = g.idEquip;
            item.meta.id_manufacturer = g.idManufacturer;
            CPacketSerializer::copyFixedString(item.meta.equip_name, sizeof(item.meta.equip_name), g.equipName);
            // ПРАВКА №3: имени производителя нет в SEquipRow - поле пустое, WARN один раз (не на каждую единицу).
            CPacketSerializer::copyFixedString(item.meta.manufacturer, sizeof(item.meta.manufacturer), "");
            for (auto& f : g.fields) item.params.push_back(f.equipFieldValueF);
            item.meta.param_amount = static_cast<uint16_t>(item.params.size());
            items.push_back(std::move(item));
        }
    if (!items.empty())
        CLogger::instance().warn("CWorker", "SEquipProxy::manufacturer не заполняется - нет источника имени в SEquipRow (см. ТЗ-6, критическая правка №3)");
    return items;
}
```

### 4.4 `filterOrgIdsToUint16`

```cpp
std::pair<std::vector<uint16_t>, size_t> CWorker::filterOrgIdsToUint16(const std::vector<uint32_t>& ids)
{
    std::vector<uint16_t> out;
    size_t dropped = 0;
    for (auto id : ids)
        if (id <= 0xFFFF) out.push_back(static_cast<uint16_t>(id));
        else dropped++;
    return {out, dropped};
}
```

---

## 5. `CAcceptor`

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include "worker.h"

class CAcceptor
{
public:
    CAcceptor(uint16_t port, std::vector<CWorker*> workers);

    void run();  // слушает listen-сокет, accept() в цикле (см. алгоритм ниже)
    void stop();

private:
    int pickWorker() const; // round-robin либо по наименьшей activeConnections() - см. ниже

    uint16_t                port;
    std::vector<CWorker*>  m_workers;
    std::atomic<size_t>      m_rrCounter{0}; // для round-robin варианта
    int                        m_listenFd{-1};
    std::atomic<bool>          m_running{false};
};
```

### Алгоритм

```
run():
    m_listenFd = socket()+bind(port)+listen()
    while m_running:
        fd = accept(m_listenFd)   // блокирующий accept - один поток, один listen-сокет, это ок
        if fd < 0: continue        // ошибка accept - лог WARN, продолжить цикл
        worker = pickWorker()
        worker->assignConnection(fd)
```

**По КРИТИЧЕСКОЙ ПРАВКЕ №2**: `CAcceptor` не принимает `CAdmissionControl` вообще и никогда не отклоняет соединение по здоровью БД — приём безусловный (см. врезку). Единственное, что регулирует нагрузку на этом этапе — `MAX_ACTIVE_CLIENTS`, применяемый позже, в `CWorker::onRequestReady` через `tryAdmit`.

`pickWorker()`: выбор реализации (round-robin через `m_rrCounter.fetch_add(1) % m_workers.size()`, либо перебор `activeConnections()` каждого воркера и выбор минимального) — оставляю на усмотрение реализации, оба варианта соответствуют `server.md` ("round-robin или по наименьшей загрузке"); по умолчанию — round-robin как более простой вариант при равной применимости.

### Критерии готовности
- Тест `pickWorker()` (round-robin): N последовательных вызовов равномерно распределяются по воркерам.
- Тест: `assignConnection` вызывается ровно один раз на каждое успешное `accept()`.

---

## 6. Что передаётся в Группу 7

- `CWorker`, `CAcceptor`, `CMainCoordinator`, `CActiveClientRegistry` — конструируются и связываются в `CServer::run()`.
- `CActiveClientRegistry` передаётся как `IActiveClientRegistry&` в `CRequestPipeline` (Группа 2) при его создании для каждого воркера (или один общий `CRequestPipeline` на сервер, если он сам по себе не имеет состояния, специфичного для соединения — проверить при сборке: `CHeaderValidator`/`CPacketParser`/`CPacketSerializer` стейтлесс, `CTimeCheckRegistry` разделяемый — один `CRequestPipeline` на весь сервер выглядит корректным, отдельного экземпляра на воркер не требуется).
- Открытый вопрос по критической правке №3 (имя производителя) — требует решения перед тем, как считать Группу 6 полностью завершённой.

## 7. Файлы для чата перед Группой 7

- `worker.h`/`.cpp`, `acceptor.h`/`.cpp`, `main_coordinator.h`/`.cpp`, `active_client_registry_impl.h`/`.cpp` — реализованные.
