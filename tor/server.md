# Multithreaded server (C++20)

дефолтная операционка клиента и сервера: FreeBSD. После полной отладки сервер может перебраться на Linux (manjaro/ubuntu) Только x86/x64
Порт сервера из командной строки, если пусто - 40000
Стандарт: C++20

## Цель: изоляция сервера MySQL.

Проект сервера является подпроектом большого проекта, поэтому некоторые данные, используемые в этом проекте неполные или вынесены в stub-файлы.

## Общий алгоритм:

Принимать запросы от клиентов (создать контекст клиента)
десериализовать заголовок запроса и инвалидировать его (если валидация не прошла - вернуть ошибку)
десериализовать и понять что клиент хочет или вернуть ошибку
использовать соответствующий шаблон SQL-запроса (в звависимости от запроса) со вставкой данных из запроса, опоросить БД
обработать результат и сериалдизовать его
отправить клиенту. ( сразу disconnect и удалить контекст клиента)

Весь протокол описан в `protocol.md`

На сервере magic = 0xDEADBEEF

## Модель потоков

TCP-соединение = ровно 1 запрос-ответ, keep-alive не поддерживается — состояние между запросами хранить не нужно.

Acceptor-поток — один поток, слушает listen-сокет, принимает входящие соединения, раздаёт fd воркер-потокам (round-robin или по наименьшей загрузке).
N воркер-потоков (по числу ядер) — каждый со своим kqueue/io_uring. Воркер ведёт жизненный цикл одного соединения: чтение запроса → валидация заголовка → десериализация → передача задачи в БД (если нужна) → приём результата → сериализация ответа → запись → закрытие соединения.
CThreadPool — для CPU-bound операций (подбор оборудования по параметрам), метрики отдаёт наружу, сам решений не принимает.
Выделенный DB-поток — единственный поток, обращающийся к CDatabaseModel.
Главный поток — координирующий узел передачи результата от DB-потока обратно нужному воркеру.
Передача результата DB → воркер

Зафиксированная схема:

Воркер, отправляя запрос в БД, генерирует внутренний request_id (уникальный в рамках своего воркера или глобально — уточняется на этапе реализации) и кладёт задачу в очередь DB-потока, указывая worker_id + request_id.
DB-поток исполняет запрос. Callback исполняется на DB-потоке и не пишет напрямую ни в один воркер.
Результат публикуется в структуру главного потока:

```cpp
struct SPendingResult
{
    uint32_t worker_id{0};
    uint32_t request_id{0};
    bool     ready{false};
    /* данные результата — тип зависит от типа запроса, определяется в блоке Query & Matching */
};

// на главном потоке:
std::mutex               g_results_mutex;
std::vector<SPendingResult> g_results; // защищено g_results_mutex
```
Главный поток (как периодическая задача в CQueue) вычитывает готовые результаты и раскладывает их по отдельным структурам каждого воркера:
```cpp
// у каждого воркера — своя пара mutex+vector
struct CWorkerInbox
{
    std::mutex                   m_mutex;
    std::vector<SPendingResult>  m_results; // готовые результаты именно для этого воркера
};
```
Каждый воркер сам периодически опрашивает свой CWorkerInbox (через таймер в своём kqueue/io_uring — например, периодическое событие с небольшим интервалом, точная величина уточняется при реализации/тюнинге), забирает готовые результаты под своим mutex и продолжает обработку соответствующего соединения (сериализация ответа → отправка → закрытие).

Итого: DB-поток → главный поток (один общий буфер) → главный поток раскладывает по persoediv-воркерным inbox'ам → воркер сам вычитывает свой inbox по таймеру. Запись в сокет всегда выполняется тем же воркер-потоком, который изначально принял соединение — кросс-поточной записи в сокет нет.

CThreadPool / CQueue

```cpp
struct STask
{
    std::chrono::steady_clock::time_point next_run;
    std::function<void()>   task;
    uint32_t  period_ms;
    void  *owner{nullptr};
    bool operator>(const STask& other) const { return next_run > other.next_run; }
};

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
};

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
};
```

CThreadPool — только исполнитель, решения не принимает.

Admission control / Graceful degradation

Выделено в отдельный блок (со своим ТЗ). Работает как периодическая задача внутри CQueue (submit(owner, task, period_ms) с ненулевым period_ms), опрашивает метрики (CThreadPool::sleepingThreads()/totalThreads(), SSystemMetrics и т.п.) и принимает решения о приёме/отклонении новых соединений или запросов. Интеграция этого блока с Server фиксируется отдельно в его собственном ТЗ.

Инвалидация заголовка

статическое поле uint16_t supported_version{0}; — выставляется в 1.

Порядок проверок:

magic == 0xDEADBEEF → иначе ERS_INVALID_MAGIC, ответ.
version == supported_version → иначе ERS_UNSUPPORTED_VERSION, ответ.
Размер входящего сообщения ≤ лимита (500 кБ зашифрованное / 200 кБ незашифрованное) → иначе разрыв без ответа.
length совпадает с реальным размером payload → иначе ERS_INVALID_LENGTH, ответ.
encrypted_flag == 1 и is_signature == 0 → ERS_ENCRYPTION_ERROR, ответ (при шифровании подпись обязана присутствовать по протоколу — используется существующий код ошибки, новый не заводим).
id_client уже активен (обрабатывается) → разрыв без ответа.
Проверка по STimeCheck (см. ниже) → при неудаче разрыв без ответа; при удаче — timestamp фиксируется.
Если is_signature == 1 — проверка подписи (заглушка) → при неудаче ERS_INVALID_SIGNATURE, ответ.
валидация по timestamp
```cpp
struct STimeCheck {
    uint16_t id_client;
    uint64_t timestamp;
};
```
Хранится в памяти, не переживает рестарт сервера. Вектор/карта, клиентов ≤ 500.

## Database

Периодическая проверка соединения с БД (MariaDB). При недоступности — ERS_DATABASE_QUERY_ERROR на все входящие запросы, попытки переподключения раз в 30 сек.

Работа с БД — описана в  `database.md`, CDatabaseModel/CAbstractConnection — готовый интерфейс, внутренняя реализация вне зоны ответственности этого блока.

Доступ к БД — строго с одного выделенного потока (библиотека не потокобезопасна).

## FreeBSD & Linux

CMakeLists.txt выставляет FreeBSD или Linux в зависимости от платформы. На Linux — io_uring.

Код, зависимый от OS, обязательно экранировать:

```cpp
#ifdef Linux

#include...

// реализация системы отслеживания на базе io_uring

#endif

#ifdef FreeBSD

// реализация системы отслеживания на базе kqueue

#endif
```
Структуры из внешних источников
```cpp
namespace NCore {
    enum EComponentTypes : int8_t
    {
        ect_water,
        ect_gas,
        ect_electric,
        ect_count
    };
}
```
