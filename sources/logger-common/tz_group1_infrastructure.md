# ТЗ — Группа 1: Инфраструктура (независимый уровень)

Статус: первая группа в порядке реализации. **Не имеет зависимостей ни от одной другой группы** — реализуется и тестируется полностью изолированно, юнит-тестами, без сети, без БД, без протокольной логики.

Входит в неё три независимых друг от друга подмодуля. Две из них можно реализовывать параллельно/в любом порядке внутри группы. А одна группа - существующая

- `logger/` — `CLogger`
- `protocol/` — wire-структуры (только POD, без валидации и без (де)сериализации — это Группа 2)
- `pool/` — `STask`, `CQueue`, `CThreadPool` - существующие классы. Из интерфес дан ниже.

Источники: `protocol.md` (структуры), `server.md` (CQueue/CThreadPool), `logger.md` (CLogger).

---

## 1.1 `logger/` — CLogger

### Требования
- Асинхронная запись (отдельный поток-писатель), безопасна для вызова из любого количества потоков одновременно.
- Уровни: `LL_TRACE, LL_DEBUG, LL_INFO, LL_WARN, LL_ERROR, LL_CRITICAL`.
- Два синка: консоль (stderr) + файл с ротацией по размеру.
- Синглтон, `init()` вызывается один раз извне (в Группе 7, при старте сервера), но модуль сам по себе не должен ничего знать о сервере.

### Интерфейс (реализовать буквально)

```cpp
// logger/log_level.h
enum class ELogLevel : uint8_t
{
    LL_TRACE,
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_CRITICAL
};

// logger/logger.h
class CLogger
{
public:
    static CLogger& instance();

    void init(const std::string& logFilePath, ELogLevel minLevel = ELogLevel::LL_INFO);
    void shutdown(); // дожидается опустошения очереди, останавливает поток-писатель

    template <typename... Args>
    void log(ELogLevel level, std::string_view component, std::string_view fmt, Args&&... args);

    template <typename... Args> void trace(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void debug(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void info(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void warn(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void error(std::string_view component, std::string_view fmt, Args&&... args);
    template <typename... Args> void critical(std::string_view component, std::string_view fmt, Args&&... args);

    CLogger(const CLogger&) = delete;
    CLogger& operator=(const CLogger&) = delete;

private:
    CLogger() = default;

    struct SLogRecord
    {
        ELogLevel        level;
        std::string      component;
        std::string      message;
        std::thread::id  threadId;
        uint64_t         timestamp; // unix epoch (секунды), как в protocol.md
    };

    void writerLoop();
    void writeToSinks(const SLogRecord&);
    void rotateIfNeeded();

    std::mutex               m_queueMutex;
    std::condition_variable   m_queueCv;
    std::deque<SLogRecord>   m_queue;
    std::thread              m_writerThread;
    std::atomic<bool>        m_running{false};

    ELogLevel                 m_minLevel{ELogLevel::LL_INFO};
    std::ofstream              m_fileSink;
    std::string                m_logFilePath;
    size_t                      m_currentFileBytes{0};
};
```

### Формат вывода
```
[YYYY-MM-DD HH:MM:SS][LEVEL][thread:0x...][component] message
```

### Ротация
```cpp
constexpr size_t LOG_FILE_MAX_BYTES   = 50 * 1024 * 1024; // 50 МБ
constexpr int    LOG_FILE_MAX_BACKUPS = 5;
```
При достижении лимита: `server.log` → `server.log.1` (сдвиг остальных, старейший — удаляется), открывается новый `server.log`.

### Что НЕ входит в эту группу
- Вызовы `CLogger::instance()` из других модулей (валидатор, БД, admission control и т.д.) — появятся по мере реализации соответствующих групп.

### Критерии готовности
- Модуль собирается и работает без каких-либо других частей проекта.
- Юнит-тест: конкурентная запись из N потоков (например 16) по M сообщений — все M×N сообщений оказываются в файле, порядок внутри одного потока сохранён.
- Тест ротации: пишем > `LOG_FILE_MAX_BYTES`, проверяем появление `server.log.1`.
- `shutdown()` гарантированно сбрасывает очередь (тест: пишем сообщение, сразу `shutdown()`, читаем файл — сообщение присутствует).

---

## 1.2 `protocol/` — wire-структуры

**Важно**: в этой группе — только объявления структур/enum'ов, побайтовая упаковка, точные размеры полей. Никакой логики чтения из сокета, валидации или (де)сериализации в объекты — это Группа 2. Цель группы 1 здесь — зафиксировать бинарный контракт как самостоятельный header, который Группа 2 будет использовать как готовый "кирпич".

Взять из `protocol.md` **дословно**, без изменений семантики:

```cpp
// protocol/wire_types.h
#pragma pack(push, 1)

enum EQueryType : uint8_t
{
    EPT_EQUIP_REQ,
    EPT_ORGS_REQ,
    EPT_EQIP_ID,
    EPT_ORGS_ID,
    EPT_COUNT
};

enum class ESignatureKeyType : uint8_t
{
    KT_NONE,
    KT_RSA_1024,
    KT_RSA_2048,
    KT_RSA_4096,
    KT_COUNT
};

enum EResponseStatus : uint16_t
{
    ERS_SUCCESS = 0,
    ERS_INVALID_MAGIC,
    ERS_UNSUPPORTED_VERSION,
    ERS_INVALID_QUERY_TYPE,
    ERS_INVALID_LENGTH,
    ERS_INVALID_SIGNATURE,
    ERS_ENCRYPTION_ERROR,
    ERS_NOT_FOUND,
    ERS_INTERNAL_ERROR,
    ERS_DATABASE_QUERY_ERROR,
    ERS_COUNT
};

struct SClientPacket
{
    uint32_t            magic{0};
    uint16_t            version{0};
    EQueryType          query_type{EPT_COUNT};
    uint16_t            id_client{0};
    uint32_t            length{0};
    uint64_t            timestamp{0};
    ESignatureKeyType   keyType{ESignatureKeyType::KT_NONE};
    uint8_t             is_signature{0};
    EResponseStatus     status{ERS_INVALID_MAGIC};
};

struct SSignature
{
    uint32_t hash{0};   // CRC-32 от SClientPacket + payload (незашифрованных)
    uint32_t length{0}; // дубликат SClientPacket::length
};

// --- payload-структуры запроса подбора оборудования ---
struct SEquipRequestData
{
    uint16_t id_component{0};
    uint16_t componentType{0};
    NCore::EComponentTypes natureType{NCore::EComponentTypes::ect_water}; // uint8_t по размеру
    uint16_t param_amount{0};
    // далее следует param_amount штук float — вне структуры, читается отдельно на этапе парсинга (Группа 2)
};

// --- payload-структуры ответа подбора оборудования ---
struct SReqHeader
{
    uint16_t component_id{0};
    uint16_t response_amount{0};
};

struct SResponseEquip
{
    uint16_t equip_id{0};
    char     name[30];
    uint16_t param_amount{0};
};

// --- payload EPT_EQIP_ID ответ ---
struct SEquipProxy
{
    uint16_t id{0};
    uint16_t id_manufacturer{0};
    char     equip_name[30];
    char     manufacturer[30];
    uint16_t param_amount{0};
};

// --- payload EPT_ORGS_REQ / EPT_ORGS_ID ответ ---
struct SOrgProxy
{
    uint32_t id{0};
    char     name[30];
    uint16_t id_type{0};
    uint16_t id_ownership{0};
    uint16_t id_country{0};
    char     inn[12];
};

#pragma pack(pop)
```

### Константы протокола (сюда же, как часть контракта)

```cpp
// protocol/limits.h
constexpr uint32_t MAX_INCOMING_ENCRYPTED_BYTES   = 500 * 1024;
constexpr uint32_t MAX_INCOMING_UNENCRYPTED_BYTES = 200 * 1024;
constexpr uint16_t PROTOCOL_MAGIC                 = 0xDEADBEEF;
constexpr uint16_t PROTOCOL_SUPPORTED_VERSION      = 1;
constexpr size_t   MAX_ACTIVE_CLIENTS              = 50;
```

### Зависимость на `NCore::EComponentTypes`
Тип задан во внешнем модуле (см. `server.md`, раздел "Структуры из внешних источников"). В рамках группы 1 просто подключается как готовый header, никакой реализации не требует.

### Критерии готовности
- `static_assert(sizeof(SClientPacket) == <точный посчитанный размер>)` — зафиксировать явным `static_assert` в коде, чтобы любое случайное изменение упаковки/порядка полей сразу ловилось на этапе компиляции.
- Аналогичные `static_assert` для `SSignature`, `SReqHeader`, `SResponseEquip`, `SEquipProxy`, `SOrgProxy`, `SEquipRequestData`.
- Модуль — чистый header/набор POD, не тянет за собой ничего, кроме `<cstdint>` и `NCore::EComponentTypes`.

---

## 1.3 `pool/` — STask, CQueue, CThreadPool
STask, CQueue, CThreadPool СУЩЕСТВУЮЩИЕ классы.
Взять из `server.md` **дословно** — интерфейс уже зафиксирован в ТЗ и менять его на этом этапе не нужно:

```cpp
// pool/task.h
struct STask
{
    std::chrono::steady_clock::time_point next_run;
    std::function<void()>   task;
    uint32_t  period_ms;
    void  *owner{nullptr};
    bool operator>(const STask& other) const { return next_run > other.next_run; }
};

// pool/queue.h
class CQueue
{
public:
    explicit CQueue(CThreadPool * pool);
    CQueue() = delete;
    CQueue(const CQueue&) = delete;
    CQueue(CQueue &&) = delete;
    ~CQueue();

    void submit(void * owner, std::function<void()> task, uint32_t period_ms); // period_ms=0 -> singleshot
    void removeTasks(void * owner);
    void start();
    void stop();

private:
    CThreadPool* m_pool;
    std::priority_queue<STask, std::vector<STask>, std::greater<>> m_tasks;
    std::mutex   m_mutex;
    std::condition_variable m_cv;
    std::thread  m_schedulerThread;
    std::atomic<bool> m_running{false};
};

// pool/thread_pool.h
class CThreadPool
{
public:
    explicit CThreadPool(size_t threads = std::thread::hardware_concurrency());
    ~CThreadPool();
    CThreadPool(const CThreadPool&) = delete;
    CThreadPool& operator=(const CThreadPool&) = delete;

    void submit(void *owner, std::function<void()> task);
    void cancelOwner(void* owner);
    void shutdown();

    size_t sleepingThreads() const;
    size_t totalThreads() const;

private:
    void workerLoop();
    void runTask(const std::function<void()>& task); // см. "Обработка исключений" ниже

    std::vector<std::thread>              m_threads;
    std::queue<std::pair<void*, std::function<void()>>> m_tasks;
    std::mutex                            m_mutex;
    std::condition_variable                m_cv;
    std::atomic<size_t>                    m_sleeping{0};
    std::atomic<bool>                      m_running{true};
};
```

### Уточнения реализации (фиксируются здесь, так как в server.md оставлены открытыми)

**`CQueue::submit` с `period_ms > 0`.** Периодическая задача перевыставляется сама: после каждого выполнения `next_run = now + period_ms`, задача остаётся в очереди до явного `removeTasks(owner)`. С `period_ms == 0` — одноразовая задача, удаляется из очереди сразу после выполнения.

**`CQueue::removeTasks(owner)`.** безопасен к вызову из любого потока, включая вызов изнутри самой задачи данного owner (используется, например, при остановке `CAdmissionControl` в будущей группе). Реализация — фильтрация очереди по `owner` под mutex.

**`CThreadPool::cancelOwner(owner)`.** Отменяет только ещё не взятые в работу задачи данного `owner` в очереди пула; уже выполняющуюся задачу не прерывает (кооперативной отмены не требуется на этом этапе).



### Критерии готовности
- `CThreadPool`: юнит-тест — submit N задач, дождаться выполнения всех (например через atomic-счётчик + condition_variable в тесте), проверить что `sleepingThreads()`/`totalThreads()` возвращают консистентные значения.
- Тест на исключение: submit задачи, бросающей `std::runtime_error` — пул не должен "потерять" поток (после этого пул продолжает выполнять новые задачи).
- `CQueue`: тест периодической задачи — submit с `period_ms=50`, проверить не менее 3 срабатываний за 200мс с допустимым дребезгом; `removeTasks` останавливает дальнейшие срабатывания.
- Тест `cancelOwner`/`removeTasks`, вызванный изнутри задачи того же owner — не должен приводить к deadlock.

---

## Что подаётся на вход Группы 2 (протокольный слой)

По завершении группы 1 наружу отдаются как готовые, стабильные интерфейсы:
- `logger/logger.h` — `CLogger::instance()`
- `protocol/wire_types.h` + `protocol/limits.h` — все wire-структуры и константы протокола
- `pool/thread_pool.h`, `pool/queue.h` — `CThreadPool`, `CQueue`

Группа 2 (`CHeaderValidator`, `CPacketParser`, `CPacketSerializer`) будет проектироваться поверх этих готовых заголовков без их изменения.
