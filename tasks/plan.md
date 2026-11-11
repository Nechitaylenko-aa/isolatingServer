# fcpp-agent — план развития

**Основа:** единое дерево `CRequestOrchestrator`. Всё развитие вытекает из него —
никаких параллельных систем, никаких обходных путей.

---

## Принцип: детерминированное дерево

Каждый запрос проходит по зафиксированной схеме. Каждый переход — следствие
факта, не угадывания:

```
normalise()
    ↓
check_request_fits()          ← текст влезает в окно хотя бы одной роли?
    ↓
choose_root()                 ← какой контекст применим?
    │
    ├─ Failure                → ExitReason (сбой оценки)
    ├─ NoContext              → route: NoApplicableContext
    ├─ History                → route: HistoryContextRequired
    ├─ File                   → route: FileContextRequired
    ├─ FileAndHistory         → route: FileAndHistoryContextRequired
    ├─ Cursor                 → route: CursorContextRequired
    └─ CursorAndHistory       → route: CursorAndHistoryContextRequired
    ↓
plan_budget() + fit_material_to_budget()
    ↓
classify_intent()             ← ThinkerLocal: Answer | Edit?
    ↓
    ├─ Edit + нет таргета     → ExitEditWithoutTarget
    ├─ Edit + нет прав        → ExitPermissionDenied
    ├─ Edit                   → generate_edit()
    └─ Answer                 → generate_answer()

generate_edit():
    take_snapshot("pre-edit")
    apply()
    take_snapshot("edit")
    [allow_exec] → build()
        → BuildFailed → revert() → ExitBuildFailed

generate_answer():
    LLM(CoderSmart | CoderLocal) → текст ответа
```

Расширение дерева — всегда добавление **нового детерминированного перехода**
в существующую схему, не новая параллельная система.

---

## Текущее состояние (что работает прямо сейчас)

- `open` → libclang-индексация → SQLite
- `ask` → полный путь Answer по всем шести маршрутам
- `ask --file` → полный путь Edit → patch → build → rollback
- `rollback --steps N / --to <sha>` → `CGitSnapshotStore` (read-tree --reset -u)
- `build`, `test`, `status`, `projects`, `doctor`, `outline`, `callers`, `callees`, `refs`

---

## Слой 1: закрыть базовые дыры (сейчас в работе)

### 1.1 `requirements.md` → архив
Файл описывает другую архитектуру (Area/Subject/Relation/Evidence Plan).
Убрать из активных задач, переименовать в `requirements_v1_archive.md`.
Этот файл (`plan.md`) — единственный живой документ плана.

### 1.2 `serve` на живом трафике
JSON-RPC `CProtocolServer` не прогонялся на реальном трафике.
Нужен ручной end-to-end: `serve` + клиент (netcat/curl) → `ask` → ответ.
**Критерий:** цикл открытие проекта → ask Answer → ask Edit → rollback
проходит по протоколу, а не через CLI `ask`.

### 1.3 `requirements.md` дрейф — мёртвые методы протокола
`build.configure`, `tidy.run`, `request.cancel` упоминаются в
`requirements.md` как методы API, но не обрабатываются в `CProtocolServer`.
После архивирования `requirements.md` — проблема закрыта сама собой.
`request.cancel` — см. Слой 2.

---

## Слой 2: расширение дерева

Каждый пункт — новый детерминированный переход. Порядок внутри слоя
не фиксирован, зависит от приоритетов.

### 2.1 Каскадный запрос — детерминированное расширение
Сейчас: один запрос → один вызов LLM → ответ.
Расширение: оркестратор сам решает, нужен ли второй проход.

Детерминированные условия для каскада (не LLM-решение):
- **Edit + build fail + stderr** → переформулировать патч с ошибкой компилятора
  как дополнительным контекстом. Максимум N попыток (конфиг).
- **Answer + индекс вернул 0 результатов** → автоматически расширить поиск
  (например, `refs` → `callers` → файл целиком), без вопроса к пользователю.

Это ReAct-lite: цикл управляется кодом, не LLM-агентом.
LLM участвует только в генерации следующего патча/ответа, не в решении
«делать ли следующий шаг».

```
generate_edit():
    for attempt in range(max_edit_attempts):
        patch = LLM(context + prev_build_error)
        apply(patch)
        if build() == OK: break
        revert(patch)
        prev_build_error = stderr
    else:
        ExitBuildFailed (исчерпаны попытки)
```

### 2.2 `request.cancel` — прерывание длинных операций
Сейчас `CRequestOrchestrator::run()` синхронный, cancel невозможен.
Нужен `m_cancelled` флаг (atomic bool) + проверки в точках между шагами
(после `choose_root`, после каждого LLM-вызова, после каждого tool-вызова).
Не прерывание системных вызовов — cooperative cancellation.

### 2.3 Детерминированные инструменты как контекст для Answer
Сейчас `generate_answer` собирает контекст из файла/курсора/истории.
Расширение: перед вызовом LLM оркестратор запрашивает индекс детерминированно
в зависимости от маршрута:
- `CursorContextRequired` → resolve symbol → `outline` + `callers` + `callees`
- `FileContextRequired` → `outline` файла

Это не Tool Registry из старого ТЗ — это фиксированная таблица
`EOrchestrationRoute → []IndexQuery` прямо в оркестраторе.

### 2.4 Несколько файлов в Edit
Сейчас `--file` — один файл. Расширение:
- `--file A --file B` → несколько файлов в контексте
- патч может затрагивать все переданные файлы
- валидация и rollback работают на весь набор

### 2.5 Streaming ответа
`generate_answer` сейчас ждёт полного ответа.
Расширение: `complete_stream` → callbacks → CLI/serve выводят токены по мере
поступления. Без изменения логики дерева — только листовой узел `generate_answer`.

---

## Слой 3: качество и наблюдаемость

### 3.1 Benchmark
Каждый реальный промах (неправильный маршрут, неверный Intent, сломанный патч)
превращается в тест-кейс. Формат:

```
request: "..."
file: src/foo.cpp (optional)
cursor: 42:5 (optional)
expected_route: CursorContextRequired
expected_intent: Edit
expected_exit: None
```

Запускается как часть `fcpp_tests`. Не отдельная система — тесты на
`CRequestOrchestrator` с фейковым LLM-слоем.

### 3.2 Мёртвый код — по фактическому прогону
После живого `serve` + benchmark: смотрим по логам и покрытию, какие ветки
`EOrchestrationRoute` / `EExitReason` реально не достигаются.
Удаляем по принципу: нашли → убедились → удалили.

### 3.3 `tidy.run`
`clang-tidy` уже проверяется в `doctor` (статус: MISSING на текущей машине).
Добавить в API как детерминированную команду — не через LLM,
напрямую из `CProtocolServer` → `CExecutor`.

### 3.4 Документация Shadow-классов (для системы проектирования)
Отдельный вопрос, не относится к `fcpp-agent` напрямую.

---

## Инварианты, которые не меняются никогда

1. **Одно дерево.** Любой запрос — один путь через `CRequestOrchestrator::run()`.
   Нет параллельных диспетчеров, нет обходных вызовов core из CLI напрямую.

2. **Переходы — по фактам, не по тексту.** `choose_root` читает наличие
   курсора/файла/истории. `classify_intent` читает ответ ThinkerLocal.
   Никаких `if (message.contains("измени"))`.

3. **LLM — листья, не оркестратор.** LLM отвечает на вопрос или генерирует патч.
   Решение «что делать дальше» принимает код.

4. **rollback всегда возможен.** Любой Edit оставляет `pre-edit` снапшот
   до записи на диск. `read-tree --reset -u` убирает в том числе новые файлы.

5. **Логирование структурное.** Каждое решение дерева — событие в `agent.jsonl`.
   Диагноз по логу, не по printf.

---

## Что не строим

- Tool Registry как отдельную подсистему (инструменты вызываются из оркестратора напрямую)
- Intent IR / Area / Subject / Relation / Projection (это из старого ТЗ)
- LLM как оркестратор выбора инструментов
- Несколько одновременных проектов (один процесс — один проект)
- GUI / IDE plugin (протокол готов, клиент — снаружи)
