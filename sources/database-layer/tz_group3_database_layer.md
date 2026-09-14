# ТЗ — Группа 3: Слой БД

Статус: третья группа, **все открытые вопросы закрыты, готово к кодогенерации**. Зависит от Группы 1 (`CLogger`) только для логирования ошибок/реконнектов. От Группы 2 не зависит вообще (см. §1 — почему). От Группы 4/6 — наоборот, Группа 3 отдаёт им готовые интерфейсы.

`CDatabaseModel`/`CAbstractConnection` — **готовый, зафиксированный интерфейс** (см. `database.md`), как раньше `CQueue`/`CThreadPool`: не редизайним, только используем. SQL-шаблоны и внутренняя реализация запросов — вне зоны этого ТЗ (`database.md`: "SQL-шаблоны — вне зоны ТЗ, инкапсулированы в CDatabaseModel").

Источники: `database.md`, `server.md` (раздел "Database", раздел "Модель потоков"), `architecture.md` (§2 директория `db/`, §6 обработка исключений).

---

## 0. Два противоречия в исходных ТЗ — нужно решение

### 0.1 Тип ID организаций в `executeOrgByID`

`protocol.md` (EPT_ORGS_ID): "Далее перечисляются ID организаций (**uint32_t**) в количестве requestAmount".

`database.md` (`CDatabaseModel`): `void executeOrgByID(std::vector<uint16_t> idList, TQueryOrgsByID callback);` — **uint16_t**.

Это прямое противоречие между протокольным и БД-слоем: `CPacketParser` (Группа 2) отдаёт `SOrgIdRequest{std::vector<uint32_t> ids}`, а `CDatabaseModel::executeOrgByID` принимает `std::vector<uint16_t>`.

**Решение (принято, Artem):** вариант **(b)**. `CDatabaseModel` — закрытая, уже реализованная зависимость (как `CQueue`/`CThreadPool`), её сигнатуру не трогаем — вариант (a) отпадает автоматически. Вариант (c) тоже отклоняю: `SOrgProxy::id` в самом протоколе (`protocol.md`/`database.md`) — `uint32_t`, то есть бизнес-пространство id организаций 32-битное на всём пути от БД до клиента; менять это ради одной сигнатуры БД-слоя неверно и рискует коллизиями на реальных данных.

Итоговое поведение (реализуется на границе Группа 6, парсер → `CDBThread`, не в этой группе): входной `std::vector<uint32_t>` (из `SOrgIdRequest`, Группа 2) фильтруется — id, не влезающие в `uint16_t` (> 65535), **отбрасываются молча для БД, но логируются** (`CLogger::warn("CDBThread"/"protocol", "org id {} truncated/dropped: exceeds uint16_t range", id)`), в `CDBThread::executeOrgByID` уходят только оставшиеся id, приведённые к `uint16_t`. Тихое усечение (`static_cast<uint16_t>`) не рассматриваю — это может случайно попасть в чужую организацию из-за коллизии по модулю 65536, что хуже, чем просто не найти запрошенную. Если после фильтрации список пуст — воркер отвечает как на `ERS_NOT_FOUND` (пустой `orgCount`), в БД вообще не ходим.

Здесь, в Группе 3, это фиксируется только как контракт: `CDBThread::executeOrgByID(std::vector<uint16_t>, ...)` **не меняется**, дословно повторяет `CDatabaseModel`. Фильтрация — обязанность вызывающей стороны (Группа 6), будет явно прописана в её ТЗ.

### 0.2 Прямой вызов `CDatabaseModel::simpleTest()` из Admission Control нарушает "один поток на БД"

`architecture.md` (§5, черновой скелет) даёт `CAdmissionControl(CQueue* queue, CDatabaseModel* db)` и вызывает `db->simpleTest()` напрямую из периодической задачи `CQueue` — то есть **не из выделенного DB-потока**. Это нарушает жёсткое требование `database.md`/`server.md`: "Доступ к БД — строго с одного выделенного потока (библиотека не потокобезопасна)".

**Решение, принимаю в рамках этой группы:** `CDBThread` сам выполняет периодическую проверку `simpleTest()` на своём собственном потоке (см. §2.3) и публикует результат в потокобезопасный атомарный флаг `isHealthy()`. `CAdmissionControl` в Группе 5 должен зависеть от `CDBThread` (читать `isHealthy()`), а **не** от `CDatabaseModel` напрямую — это меняет сигнатуру `CAdmissionControl`, зафиксированную в `architecture.md`, будет учтено при генерации ТЗ Группы 5. Здесь фиксирую только то, что относится к Группе 3: `CDBThread::isHealthy()` как единственная безопасная точка чтения состояния БД извне DB-потока.

---

## 1. Почему Группа 3 не зависит от Группы 2

`CDatabaseModel` работает с `SEquipTypeKey`/`std::vector<uint16_t>`/`std::vector<uint32_t>` — типами, определёнными в `database.md`, не в протокольном слое. Преобразование протокольных типов (`SComponentReq` из Группы 2) в `SEquipTypeKey` — по `architecture.md`, обязанность `CEquipQueryDispatcher` (Группа 4), не Группы 3. Поэтому `db/` собирается и тестируется полностью изолированно от `protocol/`.

---

## 2. Состав группы

```
db/
  db_types_fwd.h        — форвард/подключение внешнего db_types.h (см. §2.1)
  equip_row_grouping.h/.cpp  — группировка плоских SEquipRow (см. §4)
  db_config.h/.cpp       — setupDBconfig(), createConnection() (см. §2.2)
  db_thread.h/.cpp        — CDBThread (см. §2.3)
```

### 2.1 Внешние зависимости этой группы (не наш код, только используем)

- `CAbstractConnection`, `E_DB_TYPE`, `CAbstractConnection::createDatabaseInstance(...)` — готовый интерфейс, "внутренняя реализация вне зоны ответственности" (`database.md`).
- `<db_types.h>` — определяет `SDBConnection`/`SSslConf`. В `database.md` показан целиком, но физического заголовка у нас нет.
- `s_database_config` — легаси-структура, источник которой не указан (`database.md` только показывает `operator=` от неё).
- `CDatabaseModel` (класс целиком, `database.md`), `EDatabaseError`, `SEquipRow`, `SOrgProxy`, `SEquipTypeKey`.

**Что нужно от вас (см. итоговый список файлов для чата в конце документа)** — без реальных заголовков `db_types.h`/`CAbstractConnection.h` код этой группы компилируется только "по описанию" из `database.md`; для точной, а не приблизительной ТЗ-генерации следующих групп (и для собственно кодогенерации Группы 3) лучше подгрузить их в чат, если они у вас физически есть в проекте.

### 2.2 `db_config.h/.cpp` — статический конфиг

Ровно по шаблону из `database.md`, без выдумывания новых полей:

```cpp
#include <db_types.h>

void setupDBconfig(SDBConnection& config)
{
    // Жёстко зашитые значения — заполняются вручную (Artem: "статически прямо в коде забью").
    // Заготовка ниже — заполнить реальными host/user/pass/dbname перед сборкой.
    config.host   = "";  // TODO
    config.dbname = "";  // TODO
    config.user   = "";  // TODO
    config.pass   = "";  // TODO
    config.port   = 3306;
    config.ssl_config.is_enabled = false; // шифрование соединения с БД не используется на этом этапе
}

static CAbstractConnection* createConnection(const SDBConnection& connect)
{
    return CAbstractConnection::createDatabaseInstance(E_DB_TYPE::EDT_MYSQL, &connect);
}
```

Эта пара функций — единственное место в проекте, где фигурируют логин/пароль к БД (по вашему решению — захардкожено, не вынесено в файл конфигурации).

### 2.3 `CDBThread` — единственный поток доступа к БД

```cpp
#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <vector>
#include <memory>
#include <db_types.h>
#include "database_model.h" // CDatabaseModel, EDatabaseError, SEquipRow, SOrgProxy, SEquipTypeKey (из database.md)

class CDBThread
{
public:
    // Повторяют typedef'ы CDatabaseModel дословно (там они приватные - переобъявляем здесь публично,
    // чтобы вызывающая сторона могла именовать тип при необходимости; фактическое использование обычно
    // через инлайн-лямбду, так что называть тип явно не обязательно).
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

    // Все 4 метода — тонкие обёртки: кладут задачу во внутреннюю FIFO-очередь DB-потока и
    // немедленно возвращают управление. Сам вызов CDatabaseModel::executeXxx и колбэк
    // выполняются НА DB-потоке (см. server.md: "Callback исполняется на DB-потоке").
    // executeOrgByID: сигнатура uint16_t повторяет CDatabaseModel дословно (закрытая зависимость).
    // Решение по несоответствию с protocol.md (uint32_t) принято в §0.1 (вариант b): фильтрация
    // id > 65535 с логированием — обязанность вызывающей стороны (Группа 6), эта группа не меняется.
    void executeEquipQuery(std::vector<SEquipTypeKey> uniqueTypeKeys, TQueryEquipParamCallback callback);
    void executeEquipIdQuery(std::vector<uint16_t> idList, TQueryEquipIdCallback callback);
    void executeOrgByID(std::vector<uint16_t> idList, TQueryOrgsByID callback);
    void executeOrgsByType(std::vector<uint16_t> typeList, TQueryOrgsByType callback);

private:
    struct SQueuedTask { std::function<void()> run; }; // тип-стёртая обёртка над любым из 4 вызовов выше

    void threadLoop();
    void healthCheckTick();     // simpleTest() + переподключение раз в 30с при недоступности
    void reconnect();
    void runTaskSafely(const std::function<void()>& task); // try/catch, см. §3

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
```

### Алгоритм `threadLoop`

```
connection = createConnection(m_config)
model = make_unique<CDatabaseModel>(connection.get())
m_healthy = model->simpleTest()
lastHealthCheck = now()

while m_running:
    if m_healthy.load():
        task = pop from m_queue (ждём на m_queueCv, таймаут = остаток до lastHealthCheck + 30000мс)
        if task:
            runTaskSafely(task.run)
    else:
        // пока БД нездорова, к m_model/m_connection не прикасаемся вообще - только ждём
        // следующего healthCheckTick(). Задачи из m_queue НЕ вычитываются и копятся молча -
        // клиент в этом случае получит ответ позже, при восстановлении БД
        // (либо не получит вовсе, если соединение с клиентом к тому моменту закроется -
        //  это уже ответственность вызывающей стороны/Group 6, не этой группы)
        спим до (lastHealthCheck + 30000мс)
    if now() - lastHealthCheck >= 30000мс:
        healthCheckTick()
        lastHealthCheck = now()

// после выхода из цикла (stop() вызван):
// - текущая задача (если runTaskSafely уже начал её) доигрывается до конца
// - всё, что осталось в m_queue на этот момент, ОТБРАСЫВАЕТСЯ без вызова колбэка
//   (см. критерии готовности ниже - осознанное решение, Artem)
{
    std::lock_guard lock(m_queueMutex);
    m_queue.clear(); // колбэки НЕ вызываются - см. §3, аналогичный fallback как при исключении до колбэка
}
```

`healthCheckTick()`:
```
ok = m_model->simpleTest()
if ok != m_healthy.load():
    CLogger::instance().log(ok ? LL_INFO : LL_WARN, "CDBThread",
        ok ? "db health check: OK (восстановлено)" : "db health check: FAILED");
m_healthy.store(ok)
if !ok:
    reconnect() // попытка - не чаще раза в 30с (см. цикл выше)
```

**Решение по `reconnect()` (принято, Artem):** при неудачном `reconnect()` `m_connection`/`m_model` **не зануляются** — остаются прежними объектами (последний рабочий или последний, к которому была попытка подключения), просто не используются, пока `m_healthy == false` (см. правку `threadLoop` выше: ветка `else` вообще не трогает `m_model`). Гонки с самим DB-потоком (единственным, кто их использует) нет по построению — `m_model` читается/пишется только внутри `threadLoop`/`healthCheckTick`/`reconnect()`, все три работают строго последовательно на одном потоке. Явный null-check перед вызовом `m_model->executeXxx` в `runTaskSafely` не нужен: до него просто не доходит управление, пока `!m_healthy`.

**Решение по блокирующему вызову `CDatabaseModel::executeXxx(...)` (принято):** считаются **блокирующими** — коллбэк вызывается синхронно, до возврата из `executeXxx`, весь код выполняется на DB-потоке. Это следует из формулировки `server.md`: "Callback исполняется на DB-потоке" (не на каком-то внутреннем потоке библиотеки) и общего требования "доступ к БД строго с одного выделенного потока". Если фактическая реализация `CDatabaseModel` асинхронна внутри себя — `CDBThread` придётся дорабатывать, но внешний интерфейс (4 метода + `isHealthy()`) не изменится.

### Критерии готовности
- Юнит-тест на мок `CDatabaseModel`/`CAbstractConnection` (интерфейс подменяется тестовым дублем, раз он "готовый" — подойдёт любой class с такими же публичными методами, либо интерфейс-обёртка для тестов, если реальный `CAbstractConnection` не абстрактный класс, а конкретный — уточнить при получении реального заголовка, см. §2.1): submit нескольких задач подряд — выполняются строго по одной, в порядке FIFO, каждый колбэк получает управление ровно один раз.
- Тест health-check: мок, у которого `simpleTest()` сначала `true`, потом `false`, потом снова `true` — `isHealthy()` меняется соответственно, WARN/INFO пишутся в лог при каждой смене состояния (не при каждом тике).
- Тест исключения внутри задачи (см. §3) — поток DB не падает, колбэк всё равно получает `DATABASE_QUERY_FAILED`.
- Тест `stop()` во время выполнения задачи — дожидается её завершения, не убивает поток посреди работы.
- Тест `stop()` при непустой очереди — задачи, не начавшие выполняться, отбрасываются без вызова колбэка; уже стартовавшая задача доигрывается.
- Тест «БД нездорова» — пока `isHealthy() == false`, задачи из очереди не вычитываются и не исполняются (колбэк не срабатывает), `m_model`/`m_connection` не трогаются; после восстановления (`healthCheckTick` вернул `true`) накопленные задачи начинают выполняться в прежнем FIFO-порядке.

---

## 3. Обработка исключений (по `architecture.md` §6)

```cpp
void CDBThread::runTaskSafely(const std::function<void()>& task)
{
    try
    {
        task();
    }
    catch (const std::exception& e)
    {
        CLogger::instance().error("CDBThread", "unhandled exception in DB task: {}", e.what());
        // ВАЖНО: если исключение вылетело ДО того, как колбэк вызывающей стороны был вызван,
        // клиент останется без ответа - это тот samый осознанный fallback,
        // что и в CThreadPool (см. architecture.md §6). Основная гарантия ответа - на
        // задаче, которая формирует запрос (Группа 4/6), а не на этой защитной сетке.
    }
    catch (...)
    {
        CLogger::instance().error("CDBThread", "unhandled non-std exception in DB task");
    }
}
```

Это ровно такая же защитная сетка нижнего уровня, как в `CThreadPool::runTask` (Группа 1) — специально сделана идентичной по духу для консистентности во всём проекте.

---

## 4. `equip_row_grouping` — группировка плоских `SEquipRow`

`database.md`: результат запроса подбора — "плоский вектор `std::vector<SEquipRow>`... тройная группировка по: 1. natureType 2. componentEnum 3. idEquip". Это чисто механическое переформатирование данных из БД (не алгоритм подбора!) — относится к слою БД, а не к Группе 4.

**Уточнение (принято, Artem):** упорядоченность строк внутри плоского `std::vector<SEquipRow>` (по `natureType`/`componentEnum`/`idEquip`) гарантируется самой закрытой реализацией `CDatabaseModel` — она формируется соответствующим `ORDER BY` в SQL-запросе ещё до возврата результата колбэку `executeEquipQuery`. Это значит: `CDBThread::executeEquipQuery` **не меняет сигнатуру** колбэка — отдаёт наружу тот же плоский `std::vector<SEquipRow>`, что и `CDatabaseModel`, без встроенной группировки. `groupEquipRowsByType()` остаётся самостоятельной утилитой Группы 3, которую явно вызывает потребитель (по факту — Группа 4, на входе в `IEquipMatchHandler::match()`), опираясь на то, что порядок строк уже корректен благодаря БД. Хендлеры подбора (Группа 4) получают уже сгруппированные данные (`SEquipGrouped`), а не плоский список строк — но группировку вызывает сам код Группы 4, а не `CDBThread`.

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "database_model.h" // SEquipRow, SEquipTypeKey, SEquipTypeKeyHash (из database.md/architecture.md)

// Одна единица оборудования, собранная из N плоских SEquipRow с одинаковым idEquip
// (каждая строка SEquipRow - одно поле шаблона этого оборудования).
struct SEquipGrouped
{
    uint16_t     idEquip{0};
    uint16_t     idManufacturer{0};
    std::string  equipName;

    struct SField
    {
        uint16_t     idTemplate{0};
        uint16_t     idTemplateField{0};
        std::string  templFieldNameI;
        uint16_t     templFieldMeasureSetI{0};
        uint16_t     equipFieldMeasureI{0};
        uint8_t      equipFieldPrefixI{0};
        float        equipFieldValueF{0.f};
    };
    std::vector<SField> fields;
};

// Вход - плоский результат CDatabaseModel::executeEquipQuery.
// Выход - индекс: SEquipTypeKey (natureType+componentType) -> список SEquipGrouped (по idEquip).
// Строки с одинаковым (natureType, componentEnum, idEquip) схлопываются в один SEquipGrouped,
// каждая исходная строка добавляет один SField.
std::unordered_map<SEquipTypeKey, std::vector<SEquipGrouped>, SEquipTypeKeyHash>
groupEquipRowsByType(const std::vector<SEquipRow>& rows);
```

**Замечание по полю `componentType` в `SEquipTypeKey` vs `componentEnum` в `SEquipRow`:** в `database.md` строка БД называет поле `componentEnum`, а `SEquipTypeKey` (там же) — `componentType`. Считаю это одним и тем же значением под разными именами (оба — идентификатор типа компонента внутри `natureType`, ср. `E_WATER_COMPONENTS`/`E_GAS_COMPONENTS`/`E_ELECTRIC_COMPONENTS`), группировка использует `row.componentEnum` как `SEquipTypeKey::componentType`. Если это не так — пожалуйста, поправьте на этапе ревью.

### Критерии готовности
- Тест: 3 строки одного `idEquip` с разными `idTemplateField` → один `SEquipGrouped` с 3 `SField`.
- Тест: строки с разными `idEquip` в пределах одного `SEquipTypeKey` → отдельные `SEquipGrouped` в одном списке.
- Тест: строки с разными `SEquipTypeKey` → отдельные ключи в результирующей map.
- Тест на пустой вход → пустая map.
- Тест: `equipName`/`idManufacturer` берутся из первой встреченной строки данного `idEquip` (они одинаковы во всех строках одного оборудования по построению) — assert на этом инварианте в самой функции (`assert` в debug-сборке, если значения расходятся между строками одного `idEquip` — это сигнал о неконсистентности данных в БД, логировать через `CLogger::warn`, а не падать в release).

---

## 5. Что передаётся в Группу 4 / Группу 5

- **Группе 4 (подбор оборудования):** `SEquipGrouped`/`groupEquipRowsByType()` (`db/equip_row_grouping.h`) как готовый вход для `IEquipMatchHandler::match()` — сигнатура хендлера из `architecture.md` (`match(const std::vector<SEquipRow>&, ...)`) потребует правки на `match(const std::vector<SEquipGrouped>&, ...)`, будет учтено при генерации ТЗ Группы 4. `CDBThread` целиком — как единственный легальный способ сходить в БД.
- **Группе 5 (Admission Control):** `CDBThread::isHealthy()` вместо прямого `CDatabaseModel*` (см. §0.2) — меняет сигнатуру `CAdmissionControl` из черновика `architecture.md`.

---

## 6. Файлы, которые стоит вложить в чат перед генерацией ТЗ Группы 4

Обязательно, если реализация Группы 3 уже готова (как раньше с `logger.h`/`wire_types.h`):
- `db_thread.h` (или `.h/.cpp`) — реализованный `CDBThread`
- `equip_row_grouping.h` — реализованная группировка

Желательно (внешние заголовки, чтобы код Группы 3/4 писался не "по цитате из database.md", а по факту):
- `db_types.h` — если физически существует в вашем проекте (`SDBConnection`/`SSslConf`)
- заголовок с объявлением `CAbstractConnection`/`E_DB_TYPE` — если есть

Перед Группой 5 учитываем в её ТЗ (уже решено здесь, просто перенести): переход `CAdmissionControl` на `CDBThread::isHealthy()` вместо прямого `CDatabaseModel*` (§0.2).
Перед Группой 6 учитываем в её ТЗ (уже решено здесь, просто перенести): фильтрация org id > 65535 с логированием на границе парсер → `CDBThread::executeOrgByID` (§0.1, вариант b).
