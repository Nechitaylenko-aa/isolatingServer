# Карта зависимостей оркестратора (консилиум, шаг 1)

Статус: рабочий документ. Используется для хирургии: сначала фиксируем, что нужно
оркестратору, затем решаем судьбу остального. Код на этом шаге не менялся, кроме
исключения уже удалённого legacy из `CMakeLists.txt`.

## Метод

Граф построен механически, не по памяти: скрипт обошёл все `#include "..."` в
`src/` и `include/`, разрешил их в файлы и посчитал транзитивное замыкание от
точек входа бинаря (`src/main.cpp`, `src/cli/CProtocolServer.cpp`).

Ограничение метода: файлы, которые реализуют заголовок из `include/` (например
`src/core/SConfig.cpp` → `include/fcpp-agent/SConfig.h`), формально попадают в
«недостижимые», хотя нужны линковщику. Такие случаи отмечены ниже отдельно и
не считаются кандидатами на удаление.

---

## 1. Цепочка вызовов по веткам оркестратора

Точка входа: `CAgentApi::handle_ask()` → `CRequestOrchestrator::run()`.

### Общий каркас (все ветки)

```
main.cpp
  └─ CProtocolServer              (транспорт JSON)
       └─ CAgentApi               (фасад: сессия, проект, preflight)
            ├─ CConfigManager → SConfig → SLlmRoleConfig
            ├─ CConversationStore (SConversationTurn, события сессии)
            ├─ CProjectManager    (preflight: CMakeLists, compile_commands, БД)
            └─ CRequestOrchestrator
                 └─ CLogger → SLogEvent, SError, SOrchestrationTypes
```

### Ветка A — валидация входа

```
CRequestOrchestrator::run
  └─ CLogger::log(SOrchestrationRequest / SOrchestrationResult)
       └─ request_status_name / orchestration_route_name / exit_reason_name
            (SOrchestrationTypes)
```

Зависимости: `CRequestOrchestrator`, `CLogger`, `SLogEvent`, `SOrchestrationTypes`, `SError`.

### Ветка B1 — применимость курсора

```
CRequestOrchestrator::choose_root
  └─ CCursorContextAnalyzer::assess
       ├─ IClang (интерфейс)
       │    └─ CClang22  ← реальный backend, создаётся в CAgentApi
       │         (собственная зависимость: libclang)
       └─ CLlmLayer::request<bool>
            └─ CModelOpenAICompat | CModelClaude
                 ├─ CEndpointUtil
                 └─ CUrlTransport  ← libcurl
```

Зависимости: `CCursorContextAnalyzer`, `IClang`, `CClang22`, `CClangFactory`,
`CLlmLayer`, `CModelOpenAICompat`, `CModelClaude`, `CEndpointUtil`, `CUrlTransport`.

### Ветка B2 — применимость истории

```
CRequestOrchestrator::choose_root
  └─ CHistorian::assess_relevance
       └─ CLlmLayer::request<std::vector<std::size_t>>
            └─ (та же цепочка моделей, что и B1)
```

Зависимости: `CHistorian` + цепочка моделей.

### Ветка C — выбор маршрута

```
CRequestOrchestrator::run
  └─ ERootChoice → EOrchestrationRoute
       └─ SLogEvent::route_selected → CLogger
```

Зависимостей сверх каркаса нет.

### Ветки D1–D4 — генерация ответа

```
 generate_and_audit_cursor_answer      (Cursor | CursorAndHistory)
 generate_and_audit_history_answer     (History)
 generate_and_audit_default_answer     (NoApplicableContext)
  └─ CLlmLayer::request_text(role, prompt_type|system_prompt, text)
       ├─ EPromptType → встроенный system prompt
       ├─ smart_fallback: *_LOCAL → *_SMART
       └─ make_model → CModelOpenAICompat | CModelClaude
```

Зависимости: только `CLlmLayer` и провайдеры.

### Ветка E — выход и логирование

```
CRequestOrchestrator::run
  └─ CLogger::log(result, id, session, elapsed)
       └─ request.finished (status / route / exit_reason / elapsed_ms)
```

---

## 2. Минимальный набор для оркестратора

Ровно то, без чего дерево не работает:

| Слой | Файлы |
|---|---|
| Ядро дерева | `CRequestOrchestrator.h/.cpp`, `SOrchestrationTypes.h` |
| Анализ контекста | `CHistorian.h/.cpp`, `CCursorContextAnalyzer.h/.cpp` |
| Модели | `CLlmLayer.h/.cpp`, `CModelOpenAICompat.h/.cpp`, `CModelClaude.h/.cpp`, `CEndpointUtil.h/.cpp`, `CUrlTransport.h/.cpp`, `IModelApi.h` |
| Конфиг ролей | `SConfig.h`, `SLlmRoleConfig.h/.cpp`, `CConfigManager.h/.cpp` |
| Диалог | `CConversationStore.h/.cpp` |
| Логи | `CLogger.h/.cpp`, `SLogEvent.h/.cpp`, `SError.h/.cpp` |
| Компилятор | `IClang.h`, `CClang22.h/.cpp`, `CClangFactory.h/.cpp` |
| Проект/preflight | `CProjectManager.h/.cpp`, `CProjectPaths.h/.cpp`, `CProjectRegistry.h/.cpp`, `CCompileCommandsLocator.h/.cpp`, `CCompileCommandsParser.h/.cpp`, `CSchemaInitializer.h/.cpp`, `CSQLiteIndex.h/.cpp`, `CHash.h/.cpp` |
| Фасад | `CAgentApi.h/.cpp`, `CProtocolServer.h/.cpp`, `IAgentApi.h`, `main.cpp` |
| Прочие API-ветки | `CDoctor`, `CPatchParser/Validator/Applier`, `CExecutor`, `CToolContext`, `CToolRegistry`, `CIndexTool`, `CSqlCompilerIndex` |

---

## 3. Достижимо из бинаря, но НЕ через оркестратор

Это ключевой список для консилиума: код живой (его вызывает `handle_tool`,
`handle_patch`, `doctor`, `build`), но к дереву не подключён.

- `CToolContext`, `CToolRegistry`, `CIndexTool`, `CSqlCompilerIndex` — API инструментов.
  Оркестратор их не вызывает: маршруты C/D не собирают факты из индекса.
- `CExecutor` — запуск `cmake`/`ctest`; оркестратор сборкой не управляет.
- `CDoctor` — preflight инструментов; к preflight окружения не подключён.
- `CPatchParser/Validator/Applier` — ветка `patch`, вне `ask`.
- `SEvidence.h/.cpp` — модель доказательств; **никем не используется** (см. раздел 4).

Вопросы для консилиума по этому списку:

1. Может ли `CIndexTool`/`CToolRegistry` стать источником фактов для ветки D, не
   становясь владельцем маршрута?
2. Нужен ли `CDoctor` внутри preflight (проверка libclang/cmake до входа в дерево)?
3. Нужен ли `SEvidence` как контракт результата инструментов или он избыточен?

---

## 4. Недостижимо из бинаря вообще

Настоящие кандидаты на удаление или на подключение:

| Файлы | Что это | Судьба |
|---|---|---|
| `CContextManager.h/.cpp` | детерминированный отбор групп сообщений под бюджет токенов | решить |
| `CChatContextSource.h/.cpp` | бюджетная подача истории | решить |
| `CCodeContextSource.h/.cpp` | бюджетная подача кода | решить |
| `CProjectProfileContextSource.h/.cpp` | бюджетная подача паспорта проекта | решить |
| `IContextSource.h` | интерфейс трёх источников выше | решить |
| `CContextDiagnostics.h` | логирование выбора контекста | решить |
| `SEvidence.h/.cpp` | модель секций доказательств | решить |

Ложные срабатывания метода (не удалять, они нужны линковщику):
`SConfig.cpp`, `SLlmRoleConfig.cpp`, `SError.cpp`, `SLogEvent.cpp`,
`SStaleReport.cpp`, `CProjectProfile.cpp` — реализации заголовков из `include/`.

Тесты, которые сейчас проверяют недостижимый код:
`context_manager_test`, `chat_context_source_test`, `context_sources_test`.

---

## 5. Что это означает для хирургии

Фактический результат: legacy (`CReActAgent`, `CAnswerService`, `CModelCascade`,
`CRequestPipeline`, subtasks, intent, `CModelFactory`) **уже удалён** из сборки и с
диска, и на графе его нет. Осталось три решения:

1. **Контекстный бюджет** (`CContextManager` + три источника). Либо подключаем к
   веткам D (история и курсорный контекст сейчас уходят в промпт без лимита),
   либо удаляем вместе с тремя тестами. Промежуточное «висит в сборке» — худший
   вариант: тесты создают видимость покрытия.
2. **Факты из индекса** (`CToolRegistry`/`CIndexTool`). Сейчас ветка D отвечает без
   единого факта о коде. Решить, входит ли это в текущий шаг.
3. **`SEvidence`** — либо контракт результата инструментов для пункта 2, либо удалить.

Порядок разумный: сначала п.1 (он меньше и не меняет маршрут), затем п.2 как
отдельный шаг, п.3 — следствие решения по п.2.

---

## 6. Предложение по `CMakeLists.txt`

До решений держать в сборке только раздел 2 + раздел 3 (API-ветки). Раздел 4
исключить вместе с их тестами — иначе CI продолжает подтверждать поведение,
которого в рабочем пути нет. Если по п.1 решение «подключаем» — файлы остаются,
но тесты должны проверять уже реальную ветку D, а не изолированный класс.
