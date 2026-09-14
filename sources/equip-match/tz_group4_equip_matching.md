# ТЗ — Группа 4: Подбор оборудования

```cpp
// Одна единица оборудования, собранная из N плоских SEquipRow с одинаковым
// idEquip (каждая строка SEquipRow - одно поле шаблона этого оборудования).
struct SEquipGrouped
{
    uint16_t    idEquip{0};
    uint16_t    idManufacturer{0};
    std::string equipName;

    struct SField
    {
        uint16_t    idTemplate{0};
        uint16_t    idTemplateField{0};
        std::string templFieldNameI;
        uint16_t    templFieldMeasureSetI{0};
        uint16_t    equipFieldMeasureI{0};
        uint8_t     equipFieldPrefixI{0};
        float       equipFieldValueF{0.f};
    };
    std::vector<SField> fields;
};

// Одна единица оборудования в ответе EPT_EQUIP_REQ
struct SEquipRespItem
{
    SResponseEquip     meta;
    std::vector<float> params;
};
```

Статус: четвёртая группа. Зависит от Группы 1 (`CLogger`, `CThreadPool`), Группы 2 (`SComponentReq`, `SEquipRespItem`/`SEquipReqRespBlock` из `protocol/packet_serializer.h`, `EResponseStatus`), Группы 3 (`CDBThread`, `SEquipGrouped`/`groupEquipRowsByType`, `SEquipTypeKey`/`SEquipTypeKeyHash`).

Алгоритм подбора внутри хендлеров — реализация пользователя, вне зоны этого ТЗ. Здесь фиксируется только интерфейс хендлера, реестр и диспетчер.

Источники: `architecture.md` §4, `database.md` (раздел "Логика запроса подбора оборудования"), реализованные `db_thread.h`, `db_config.h`.

---

## 0. Правки относительно черновика `architecture.md` §4

1. `IEquipMatchHandler::match()` принимает `const std::vector<SEquipGrouped>&` (Группа 3), а не `const std::vector<SEquipRow>&` — сырые строки БД хендлеру не отдаются, группировка уже сделана.
2. `match()` возвращает `std::vector<SEquipRespItem>` (Группа 2, `protocol/packet_serializer.h`), а не `std::vector<SResponseEquip>` — сразу с параметрами (`SResponseEquip` без значений float бесполезен как результат подбора).
3. `CEquipQueryDispatcher::dispatch()` не принимает `worker_id`/`connection_ctx` — маршрутизация к конкретному воркеру/соединению инкапсулирована в самом колбэке `onComplete`, который передаёт вызывающая сторона (Группа 6), по аналогии с тем, как `CDBThread` не знает про `worker_id`/`request_id`.
4. `onComplete` получает `EResponseStatus` первым параметром — при ошибке БД (`EDatabaseError != DATABSE_OK`) диспетчер обязан вернуть `ERS_DATABASE_QUERY_ERROR` с пустым результатом, а не пытаться собрать частичный ответ.

---

## 1. Состав группы

```
equip/
  equip_match_handler.h        — IEquipMatchHandler
  equip_handler_registry.h/.cpp — CEquipHandlerRegistry
  equip_query_dispatcher.h/.cpp  — CEquipQueryDispatcher
  handlers/                      — конкретные хендлеры (CWaterFilterLightHandler и т.п.) - НЕ входят в это ТЗ,
                                    реализуются пользователем отдельно по интерфейсу IEquipMatchHandler
```

---

## 2. `IEquipMatchHandler`

```cpp
#pragma once
#include <vector>
#include "parsed_types.h"        // SComponentReq (Группа 2)
#include "equip_row_grouping.h"  // SEquipGrouped (Группа 3)
#include "packet_serializer.h"    // SEquipRespItem (Группа 2)

class IEquipMatchHandler
{
public:
    virtual ~IEquipMatchHandler() = default;

    // equipPool - оборудование ОДНОГО SEquipTypeKey (natureType+componentType), из БД, уже сгруппированное
    //             по idEquip (см. groupEquipRowsByType, Группа 3)
    // request   - параметры конкретного компонента из запроса клиента (SComponentReq::parameters)
    // Возвращает отобранные единицы оборудования, готовые к сериализации (SResponseEquip уже заполнен
    // внутри SEquipRespItem, включая param_amount = params.size()).
    //
    // Реализация - целиком на усмотрение автора хендлера. Единственное требование интерфейса:
    // метод не должен бросать исключения наружу как штатный способ сообщить "ничего не найдено" -
    // для этого случая возвращается пустой vector. Диспетчер (см. §4) оборачивает вызов в try/catch
    // как защитную сетку, но рассчитывать на неё как на механизм управления потоком нельзя.
    virtual std::vector<SEquipRespItem> match(const std::vector<SEquipGrouped>& equipPool,
                                                 const SComponentReq& request) = 0;
};
```

---

## 3. `CEquipHandlerRegistry`

Дословно по `architecture.md` §4, без изменений:

```cpp
#pragma once
#include <memory>
#include <unordered_map>
#include "database_model.h" // SEquipTypeKey
#include "equip_match_handler.h"

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
    IEquipMatchHandler* find(const SEquipTypeKey& key) const; // nullptr, если не зарегистрирован

private:
    std::unordered_map<SEquipTypeKey, std::unique_ptr<IEquipMatchHandler>, SEquipTypeKeyHash> m_handlers;
};
```

`registerHandler` с уже занятым ключом — перезаписывает предыдущий хендлер (лог `WARN` через `CLogger`, component `"CEquipHandlerRegistry"`), не бросает исключение. Регистрация конкретных реализаций — в Группе 7 (bootstrap при старте сервера), не здесь.

### Критерии готовности
- Тест: регистрация + `find()` по тому же ключу возвращает тот же указатель.
- Тест: `find()` незарегистрированного ключа → `nullptr`.
- Тест: повторная регистрация одного ключа → второй хендлер побеждает, `find()` возвращает его.

---

## 4. `CEquipQueryDispatcher`

```cpp
#pragma once
#include <functional>
#include <vector>
#include "database_model.h"     // SEquipTypeKey, EDatabaseError
#include "db_thread.h"            // CDBThread
#include "thread_pool.h"           // CThreadPool
#include "parsed_types.h"           // SComponentReq
#include "packet_serializer.h"       // SEquipReqRespBlock, SEquipRespItem
#include "wire_types.h"                // EResponseStatus
#include "equip_handler_registry.h"

using TDispatchComplete = std::function<void(EResponseStatus status, std::vector<SEquipReqRespBlock> blocks)>;

class CEquipQueryDispatcher
{
public:
    CEquipQueryDispatcher(CEquipHandlerRegistry& registry, CThreadPool& pool, CDBThread& dbThread);

    // components - результат парсинга EPT_EQUIP_REQ (Группа 2), блоки с param_amount=0 уже отфильтрованы.
    // onComplete  - вызывается РОВНО ОДИН РАЗ, на потоке CThreadPool (последний завершившийся компонент)
    //               либо на DB-потоке (в случае немедленной ошибки БД) либо синхронно из dispatch()
    //               (в случае пустого components - см. алгоритм ниже).
    void dispatch(std::vector<SComponentReq> components, TDispatchComplete onComplete);

private:
    CEquipHandlerRegistry& m_registry;
    CThreadPool&            m_pool;
    CDBThread&               m_dbThread;
};
```

### 4.1 Алгоритм `dispatch`

```
if components.empty():
    onComplete(ERS_SUCCESS, {})   // вызывается синхронно, до возврата из dispatch()
    return

# 1. Вычленить уникальные SEquipTypeKey
uniqueKeys = []
seen = set<SEquipTypeKey>()
for c in components:
    key = SEquipTypeKey{c.natureType, c.componentType}
    if key not in seen: seen.insert(key); uniqueKeys.push_back(key)

# 2. Запрос в БД (асинхронно относительно вызывающего потока, см. CDBThread)
m_dbThread.executeEquipQuery(uniqueKeys, [this, components = std::move(components), onComplete](
        std::vector<SEquipRow> rows, EDatabaseError error) {
    onDbResult(std::move(rows), error, components, onComplete);
});
```

### 4.2 `onDbResult` (выполняется на DB-потоке — колбэк `CDBThread`, см. ТЗ Группы 3)

```
if error != DATABSE_OK:
    CLogger::instance().warn("CEquipQueryDispatcher", "db query failed: error={}", (int)error);
    onComplete(ERS_DATABASE_QUERY_ERROR, {});
    return

grouped = groupEquipRowsByType(rows)   // Группа 3

# Общий контекст для сборки результата - переживает все асинхронные задачи пула
ctx = make_shared<SDispatchContext>();
ctx->blocks.resize(components.size());       // порядок = порядок components, 1:1
ctx->remaining.store(components.size());
ctx->onComplete = onComplete;

for i, comp in enumerate(components):
    key = SEquipTypeKey{comp.natureType, comp.componentType}
    handler = m_registry.find(key)

    if handler == nullptr:
        CLogger::instance().warn("CEquipQueryDispatcher",
            "handler not found for natureType={} componentType={}", (int)comp.natureType, comp.componentType);
        ctx->blocks[i] = SEquipReqRespBlock{ SReqHeader{comp.id_component, 0}, {} };
        finishOne(ctx);   // см. 4.3
        continue

    pool_it = grouped.find(key)
    pool = (pool_it != grouped.end()) ? pool_it->second : std::vector<SEquipGrouped>{}

    m_pool.submit(this, [this, ctx, i, comp, handler, pool = std::move(pool)]() {
        std::vector<SEquipRespItem> items;
        try {
            items = handler->match(pool, comp);
        } catch (const std::exception& e) {
            CLogger::instance().error("CEquipQueryDispatcher",
                "handler threw for natureType={} componentType={}: {}",
                (int)comp.natureType, comp.componentType, e.what());
            items.clear();
        } catch (...) {
            CLogger::instance().error("CEquipQueryDispatcher",
                "handler threw non-std exception for natureType={} componentType={}",
                (int)comp.natureType, comp.componentType);
            items.clear();
        }
        ctx->blocks[i] = SEquipReqRespBlock{
            SReqHeader{ comp.id_component, static_cast<uint16_t>(items.size()) },
            std::move(items)
        };
        finishOne(ctx);
    });
```

### 4.3 `finishOne` — синхронизация завершения

```cpp
struct SDispatchContext
{
    std::vector<SEquipReqRespBlock> blocks;
    std::atomic<size_t>              remaining;
    TDispatchComplete                 onComplete;
};

void finishOne(std::shared_ptr<SDispatchContext> ctx)
{
    if (ctx->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        // это был последний компонент
        ctx->onComplete(ERS_SUCCESS, std::move(ctx->blocks));
    }
}
```

`fetch_sub` возвращает значение ДО декремента — `== 1` означает, что именно этот вызов обнулил счётчик, и только он вызывает `onComplete` (гарантия ровно одного вызова при любом порядке завершения потоков пула).

### Замечание по потокобезопасности `ctx->blocks`
Каждая задача пишет только в свой индекс `i` (`ctx->blocks[i] = ...`), индексы не пересекаются — гонок по данным нет без дополнительных мьютексов. Сам `std::vector` не меняет размер после `resize()` в `onDbResult` (никаких `push_back` из задач пула) — переаллокаций во время параллельной записи не происходит.

### Критерии готовности
- Тест: `components` с 2 разными `SEquipTypeKey`, оба хендлера зарегистрированы, оба возвращают непустой результат — `onComplete` вызывается один раз с `ERS_SUCCESS` и 2 блоками в исходном порядке `components`.
- Тест: один из ключей без хендлера — соответствующий блок `response_amount=0`, `items` пуст, остальные блоки не затронуты, `onComplete` всё равно вызывается один раз после завершения остальных.
- Тест: `executeEquipQuery` возвращает `error != DATABSE_OK` — `onComplete(ERS_DATABASE_QUERY_ERROR, {})`, `CThreadPool` вообще не используется (0 submit).
- Тест: хендлер бросает исключение — соответствующий блок получает `response_amount=0`, лог `ERROR`, остальные компоненты обрабатываются нормально, `onComplete` вызывается один раз.
- Тест: `components.empty()` — `onComplete(ERS_SUCCESS, {})` вызывается синхронно внутри `dispatch()`, без обращения к `CDBThread`/`CThreadPool`.
- Тест на реальном `CThreadPool` (не моке): 20+ компонентов, часть с одинаковым `SEquipTypeKey` (общий `equipPool`, но разные `parameters`) — все 20 блоков собраны корректно, порядок соответствует входному `components`, гонок не обнаружено (ASAN/TSAN).

---


