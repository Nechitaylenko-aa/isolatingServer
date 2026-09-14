# Архитектура сервера

Основано на `protocol.md`, `server.md`, `database.md`. Данный документ фиксирует итоговую структуру классов и потоков, закрывает открытые вопросы из ТЗ.

---

## 1. Общая схема потоков

```
                      ┌────────────────────┐
   clients  ───TCP───►│   Acceptor-поток    │
                      │  (listen-сокет)     │
                      └─────────┬───────────┘
                                │ round-robin / least-loaded
                                ▼
      ┌─────────────────────────────────────────────────┐
      │            Worker-поток #0 .. #N-1                │
      │  (kqueue / io_uring, свой event-loop)             │
      │  читает запрос → валидирует заголовок →           │
      │  десериализует → формирует задачу →                │
      │  ждёт результат (CWorkerInbox) →                   │
      │  сериализует ответ → пишет в сокет → закрывает     │
      └───────┬───────────────────────────┬───────────────┘
              │ CPU-bound (подбор)         │ DB-bound
              ▼                             ▼
      ┌───────────────┐            ┌──────────────────────┐
      │  CThreadPool   │            │   Главный поток        │
      │ (матчинг       │            │ (координатор DB→worker)│
      │  оборудования) │            └──────────┬────────────┘
      └────────────────┘                       │
                                                 ▼
                                        ┌──────────────────┐
                                        │   DB-поток          │
                                        │  CDatabaseModel     │
                                        └──────────────────┘
```

Плюс сквозная периодическая задача **CAdmissionControl**, работающая через `CQueue`, не привязанная к конкретному воркеру.

---

## 2. Классы верхнего уровня

```cpp
class CServer
{
public:
    CServer(uint16_t port, size_t workerCount = std::thread::hardware_concurrency());
    void run();
    void stop(); // TODO: graceful shutdown — будет проработан вместе с полновесным Admission control

private:
    CAcceptor                    m_acceptor;
    std::vector<std::unique_ptr<CWorker>> m_workers;
    CThreadPool                  m_matchPool;      // для подбора оборудования
    CDBThread                    m_dbThread;        // единственный поток работы с БД
    CMainCoordinator              m_coordinator;     // DB -> worker inbox
    CQueue                       m_systemQueue;     // periodic tasks (admission control и т.п.)
    CAdmissionControl             m_admissionControl;
    CLogger                      m_logger;          // см. logger.md
};
```

### CAcceptor

Один поток, слушает listen-сокет, перед `accept()` спрашивает `CAdmissionControl::isAcceptingConnections()`.
Если отказ — соединение принимается и сразу закрывается без ответа (на этой стадии заголовок ещё не читался, отвечать нечем и незачем).
Если приём разрешён — fd передаётся воркеру (round-robin, либо по наименьшей загрузке — простой счётчик активных соединений на воркер).

### CWorker

Инкапсулирует один event-loop (`kqueue`/`io_uring`, экранируется как в `server.md`). Ведёт жизненный цикл соединения от чтения до закрытия. Одно TCP-соединение = ровно один цикл, состояние между запросами не хранится (см. `protocol.md`).

Обязанности воркера:
1. Прочитать сообщение (учитывая лимиты 500кБ/200кБ, см. `server.md`).
2. Провалидировать заголовок (`CHeaderValidator`) в порядке, зафиксированном в `server.md`.
3. Десериализовать payload (`CPacketParser`) → получить типизированный запрос.
4. В зависимости от типа запроса:
   - `EPT_EQUIP_REQ` → передать в `CEquipQueryDispatcher` (DB-запрос уникальных типов + затем матчинг в `CThreadPool`);
   - `EPT_EQIP_ID`, `EPT_ORGS_REQ`, `EPT_ORGS_ID` → напрямую в DB-поток через координатор.
5. Периодически (по таймеру своего event-loop) опрашивать свой `CWorkerInbox`.
6. При получении готового результата — сериализовать ответ (`CPacketSerializer`) и записать в сокет, затем закрыть соединение и удалить контекст клиента.

**Лимит одновременных клиентов: 50** (константа `MAX_ACTIVE_CLIENTS = 50`). Проверяется вместе с проверкой "id_client уже активен" из `server.md`. Впоследствии значение будет вычисляться `CAdmissionControl` динамически — см. раздел 5.

---

## 3. Передача задач в БД: `request_id`

Решение: **request_id — монотонный атомарный счётчик внутри каждого воркера** (`std::atomic<uint32_t>` — поле воркера, инкрементируется на каждый исходящий DB-запрос). Уникальность в рамках системы обеспечивается парой `(worker_id, request_id)`, глобальный счётчик не нужен — это проще и не требует синхронизации между воркерами.

```cpp
struct SDBTaskId
{
    uint32_t worker_id;
    uint32_t request_id; // локальный монотонный счётчик воркера
};
```

Обнуление счётчика при переполнении не критично — `worker_id` не меняется, а коллизия `request_id` возможна только если воркер держит 2^32 незавершённых запросов одновременно, что невозможно при лимите 50 клиентов на весь сервер.

### CWorkerInbox — таймер опроса

Период опроса **не фиксируется на этом этапе** ("уточняется при тюнинге"), выносится в константу:

```cpp
constexpr uint32_t WORKER_INBOX_POLL_MS = 5; // TODO: подобрать эмпирически при нагрузочном тестировании
```

---

## 4. Хендлеры подбора оборудования

Алгоритм подбора и внутренняя логика **вне зоны этого документа** — реализуется пользователем самостоятельно. Здесь фиксируется только интерфейс и механизм диспетчеризации, чтобы алгоритмы можно было подключать по типу компонента независимо друг от друга.

```cpp
// Абстрактный хендлер подбора для одной пары (natureType, componentType)
class IEquipMatchHandler
{
public:
    virtual ~IEquipMatchHandler() = default;

    // equipPool  — оборудование данного типа, полученное из БД (уже сгруппированное по idEquip)
    // request    — параметры конкретного компонента из запроса клиента
    // Возвращает готовые к сериализации записи (SResponseEquip + значения параметров)
    virtual std::vector<SResponseEquip> match(const std::vector<SEquipRow>& equipPool,
                                                const SComponentReq& request) = 0;
};
```

### Реестр хендлеров

```cpp
struct SEquipTypeKeyHash
{
    size_t operator()(const SEquipTypeKey& k) const noexcept
    {
        return (static_cast<size_t>(k.natureType) << 16) ^ k.componentType;
    }
};

class CEquipHandlerRegistry
{
public:
    void registerHandler(SEquipTypeKey key, std::unique_ptr<IEquipMatchHandler> handler);
    IEquipMatchHandler* find(const SEquipTypeKey& key) const; // nullptr если не зарегистрирован

private:
    std::unordered_map<SEquipTypeKey, std::unique_ptr<IEquipMatchHandler>, SEquipTypeKeyHash> m_handlers;
};
```

Регистрация конкретных реализаций (`CWaterFilterLightHandler`, `CGasFilterHandler`, ...) производится один раз при старте сервера в `CServer` (или отдельном `bootstrap`-модуле) — по одному хендлеру на каждый `E_WATER_COMPONENTS` / `E_GAS_COMPONENTS` / `E_ELECTRIC_COMPONENTS`, которые реально поддерживаются.

### Диспетчер (склеивает protocol.md + database.md)

```cpp
class CEquipQueryDispatcher
{
public:
    CEquipQueryDispatcher(CEquipHandlerRegistry& registry, CThreadPool& pool);

    // 1. на входе — вектор SComponentReq (после десериализации запроса воркером)
    // 2. вычленяет уникальные SEquipTypeKey -> уходит DB-запрос (через координатор/DB-поток)
    // 3. по получении std::vector<SEquipRow> — группирует по SEquipTypeKey
    // 4. для каждого SComponentReq находит хендлер и сабмитит матчинг в CThreadPool
    // 5. по завершении всех хендлеров для запроса — собирает SReqHeader+SResponseEquip и уведомляет воркер
    void dispatch(uint32_t worker_id, uint32_t connection_ctx,
                   std::vector<SComponentReq> components,
                   std::function<void(std::vector<SReqHeader>, std::vector<SResponseEquip>)> onComplete);

private:
    CEquipHandlerRegistry& m_registry;
    CThreadPool&            m_pool;
};
```

Если хендлер для `SEquipTypeKey` не зарегистрирован — соответствующий компонент получает `response_amount = 0` (оборудование не найдено), в лог пишется предупреждение (см. `logger.md`), сервер не падает.

---

## 5. Admission control (скелет)

Текущее ТЗ — простое: периодически проверять живость БД. Остальное (метрики CThreadPool, динамический лимит соединений) — задел на будущее.

```cpp
class CAdmissionControl
{
public:
    CAdmissionControl(CQueue* queue, CDatabaseModel* db);

    void start(); // submit(this, [this]{ checkHealth(); }, HEALTH_CHECK_PERIOD_MS)
    void stop();  // queue->removeTasks(this)

    bool isAcceptingConnections() const noexcept { return m_dbHealthy.load(); }
    size_t maxActiveConnections() const noexcept { return m_maxConnections.load(); } // пока = 50, статично

private:
    void checkHealth(); // db->simpleTest(); обновляет m_dbHealthy; логирует смену состояния

    CQueue*             m_queue;
    CDatabaseModel*      m_db;
    std::atomic<bool>    m_dbHealthy{true};
    std::atomic<size_t>  m_maxConnections{50}; // TODO: заменить динамическим расчётом, когда блок станет полновесным

    static constexpr uint32_t HEALTH_CHECK_PERIOD_MS = 30000; // синхронно с переподключением БД из server.md
};
```

Пока при недоступности БД сервер, согласно `server.md`, продолжает принимать соединения и отвечает `ERS_DATABASE_QUERY_ERROR` на все запросы — `isAcceptingConnections()` в текущей простой реализации **не блокирует приём**, а лишь публикует состояние для использования обработчиками запросов (чтобы не дёргать БД зря, если она заведомо недоступна). Полноценная блокировка приёма соединений на уровне acceptor — предмет доработки, когда Admission control станет полновесным.

---

## 6. Обработка исключений

Два уровня защиты:

**Уровень задачи (обязанность автора хендлера/логики).** Каждая задача, отправляемая в `CThreadPool` или выполняемая в DB-callback, обязана сама поймать свои ожидаемые исключения и превратить их в валидный результат (например, `ERS_INTERNAL_ERROR` + пустой payload), чтобы клиент гарантированно получил ответ, а не завис на таймауте.

**Уровень пула — защитная сетка.** `CThreadPool::submit` оборачивает вызов пользовательской задачи в `try/catch` как последний рубеж — предотвращает падение рабочего потока пула целиком, если задача всё же выбросила необработанное исключение:

```cpp
void CThreadPool::runTask(const std::function<void()>& task)
{
    try
    {
        task();
    }
    catch (const std::exception& e)
    {
        CLogger::instance().error("CThreadPool", "unhandled exception in task: {}", e.what());
        // клиент в этом случае может не получить ответ — это осознанный fallback,
        // основная гарантия ответа лежит на самой задаче (см. выше)
    }
    catch (...)
    {
        CLogger::instance().error("CThreadPool", "unhandled non-std exception in task");
    }
}
```

Аналогичная обёртка — на уровне DB-callback в `CDBThread`: исключение из `CDatabaseModel`/драйвера не должно уронить DB-поток, оборачивается в `EDatabaseError::DATABASE_QUERY_FAILED` (или новый код при необходимости) и уходит штатным путём через `SPendingResult`.

Больше здесь фиксировать нечего — конкретная логика восстановления/повторов внутри отдельных хендлеров остаётся на усмотрение реализации.

---

## 7. Логирование

Вынесено в отдельный файл `logger.md`. Кратко: единый `CLogger`, асинхронный, уровни TRACE..CRITICAL, используется из всех компонентов выше (валидация заголовка, admission control, исключения в пуле, ошибки БД, реестр хендлеров).

---

## 8. Предлагаемая структура каталогов

```
src/
  protocol/        // SClientPacket, сериализация/десериализация, CHeaderValidator, CPacketSerializer
  net/              // CAcceptor, CWorker, CWorkerInbox
  pool/             // CThreadPool, CQueue, STask
  db/                // CDBThread, CDatabaseModel, CAbstractConnection (готовый интерфейс)
  equip/            // IEquipMatchHandler, CEquipHandlerRegistry, CEquipQueryDispatcher, конкретные хендлеры
  admission/        // CAdmissionControl
  logger/           // CLogger (см. logger.md)
  server/           // CServer, CMainCoordinator, точка входа main()
```

---

## 9. Открытые вопросы (осознанно не закрыты сейчас)

- Полновесный Admission control (динамический лимит, метрики CThreadPool) — отдельное ТЗ.
- Graceful shutdown — вместе с п. выше.
- Точное значение `WORKER_INBOX_POLL_MS` — по результатам нагрузочного тестирования.
- SQL-шаблоны — вне зоны ТЗ, инкапсулированы в `CDatabaseModel`.
