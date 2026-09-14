# ТЗ — Группа 5: Admission Control (скелет)

Статус: пятая группа. Зависит от Группы 1 (`CQueue`, `CLogger`), Группы 3 (`CDBThread::isHealthy()`), Группы 2/1 (`MAX_ACTIVE_CLIENTS` из `limits.h`).

Скелет: только периодическая проверка живости БД и публикация состояния. Динамический лимит соединений по метрикам `CThreadPool` — вне зоны этого ТЗ (см. `architecture.md` §9).

---

## 0. Правки относительно черновика `architecture.md` §5

1. Конструктор принимает `CDBThread&`, а не `CDatabaseModel*` — согласно решению из ТЗ Группы 3 (§0.2): `CDatabaseModel` не потокобезопасен, `simpleTest()` вызывается исключительно на DB-потоке самим `CDBThread`. `CAdmissionControl` только читает атомарный `isHealthy()`, никогда не обращается к `CDatabaseModel` напрямую.
2. Период опроса `CAdmissionControl` (чтение `isHealthy()`) отделён от периода реконнекта внутри `CDBThread` (30с, `DB_RECONNECT_PERIOD_MS`): чтение атомарного флага — дешёвая операция, опрашивается раз в 1с (`ADMISSION_HEALTH_POLL_MS`), чтобы смена состояния логировалась с минимальной задержкой, не совпадающей с 30-секундным циклом самого `CDBThread`.
3. `isAcceptingConnections()` отражает `isHealthy()` дословно, но **использование** этого значения в `CAcceptor` — решение Группы 6. Согласно `server.md`/черновику `architecture.md` §5, на текущем этапе приём соединений не блокируется недоступностью БД (сервер продолжает принимать и отвечает `ERS_DATABASE_QUERY_ERROR` на запросы) — значение публикуется, а не диктует поведение `CAcceptor`.

---

## 1. Состав группы

```
admission/
  admission_control.h/.cpp  — CAdmissionControl
```

---

## 2. `CAdmissionControl`

```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include "queue.h"       // CQueue (Группа 1)
#include "db_thread.h"    // CDBThread (Группа 3)
#include "limits.h"        // MAX_ACTIVE_CLIENTS (Группа 1/2)

class CAdmissionControl
{
public:
    CAdmissionControl(CQueue& queue, CDBThread& dbThread);
    ~CAdmissionControl();

    CAdmissionControl(const CAdmissionControl&) = delete;
    CAdmissionControl& operator=(const CAdmissionControl&) = delete;

    void start(); // queue.submit(this, [this]{ checkHealth(); }, ADMISSION_HEALTH_POLL_MS)
    void stop();  // queue.removeTasks(this)

    // Безопасно из любого потока.
    bool   isAcceptingConnections() const noexcept { return m_dbHealthy.load(std::memory_order_acquire); }
    size_t maxActiveConnections() const noexcept    { return m_maxConnections.load(std::memory_order_acquire); }

private:
    void checkHealth(); // читает m_dbThread.isHealthy(), логирует ТОЛЬКО смену состояния

    CQueue&              m_queue;
    CDBThread&            m_dbThread;

    std::atomic<bool>     m_dbHealthy{true};
    std::atomic<size_t>   m_maxConnections{MAX_ACTIVE_CLIENTS}; // TODO (вне этого ТЗ): динамический расчёт

    std::atomic<bool>     m_started{false};

    static constexpr uint32_t ADMISSION_HEALTH_POLL_MS = 1000;
};
```

### 2.1 `checkHealth()`

```cpp
void CAdmissionControl::checkHealth()
{
    const bool healthy = m_dbThread.isHealthy();
    const bool prev     = m_dbHealthy.exchange(healthy, std::memory_order_acq_rel);

    if (healthy != prev)
    {
        if (healthy)
            CLogger::instance().info("CAdmissionControl", "db health check: OK (восстановлено)");
        else
            CLogger::instance().warn("CAdmissionControl", "db health check: FAILED");
    }
}
```

Никакого прямого обращения к `CDatabaseModel`/`CAbstractConnection` — весь модуль работает только через `CDBThread::isHealthy()`.

### 2.2 `start()`/`stop()`

```cpp
void CAdmissionControl::start()
{
    if (m_started.exchange(true)) return; // повторный start() - no-op
    m_queue.submit(this, [this]{ checkHealth(); }, ADMISSION_HEALTH_POLL_MS);
}

void CAdmissionControl::stop()
{
    if (!m_started.exchange(false)) return; // повторный stop()/stop() без start() - no-op
    m_queue.removeTasks(this);
}
```

Деструктор вызывает `stop()`, если ещё не остановлен (симметрично с `CDBThread::~CDBThread()`).


## 3. Что передаётся в Группу 6

- `CAdmissionControl::isAcceptingConnections()` и `maxActiveConnections()` — доступны `CAcceptor` для чтения (по ссылке/указателю), с оговоркой из §0.3: на этом этапе `CAcceptor` **не обязан** блокировать приём по `isAcceptingConnections()`, значение публикуется на будущее.
- Экземпляр `CAdmissionControl` создаётся и стартует в `CServer::run()` (Группа 7), но передаётся в `CAcceptor` уже в рамках сборки Группы 6.


