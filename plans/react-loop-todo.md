# fcpp-agent: детальный TODO — LLM-driven loop (ReAct)

> **СТАТУС: РЕАЛИЗОВАНО и ПРОВЕРЕНО (Фазы 0–4).** Включено по умолчанию (`react_loop="YES"`).
> E2E на локальной `Qwen2.5-Coder-14B`:
> - простой: модель сама `search → outline → source` → верный ответ;
> - сложный B-002 (курсор внутри Shape): модель сама `derived → overrides` →
>   «Все методы у наследников Circle и Rectangle реализованы».
>
> Осталось: Фаза 5 (формальные тесты), Фаза 6 (ADR-23 + docs).

Статус: ПЛАН. Основание — эксперимент [`plans/local-model-loop-test.md`](local-model-loop-test.md):
локальная `Qwen2.5-Coder-14B` **нативно** поддерживает `tools`/`tool_calls`, многошаговость
и построение цепочек (`derived → outline → …`). Гипотеза подтверждена.

## Цель
Дать **модели управлять loop'ом**: LLM решает, какой детерминированный tool вызвать,
видит результат и продолжает, пока не соберёт факты и не ответит. Наши tools/индекс остаются
исполнителем. `Intent/planner/Evidence` становятся **fast-path** (не единственным путём).

## Ключевой принцип
- **Решает LLM** — какие шаги и когда остановиться.
- **Исполняет детерминированный core** — tools, индекс, clang.
- **Транспорт** — уже готов (`serve`, JSON-RPC stdio).
- Ничего из существующего не ломаем: loop — **надстройка** за флагом.

---

## Фаза 0. Подготовка (инфраструктура loop'а)

### 0.1. Флаг режима
- [ ] В `SConfig` добавить `m_react_loop` (YES/NO, **дефолт YES** — иначе агент неработающий).
- [ ] Ключ `react_loop="YES"` в `agent.conf` + дефолт в `CConfigManager` (bootstrap/merge).
- [ ] При `NO` — текущий детерминированный путь (fast-path), как раньше.

### 0.2. Границы применимости
- [ ] Loop включается только для `ask` (пользовательские вопросы).
- [ ] НЕ включать для детерминированных CLI-команд (`outline`, `build` и т.д.).

---

## Фаза 1. Tool-протокол в моделях

### 1.1. DTO tool-calling (`IModelApi.h`)
- [ ] `SModelToolParam { name, type, description, required }`.
- [ ] `SModelTool { name, description, params[] }` — схема инструмента.
- [ ] `SModelToolCall { id, name, arguments_json }` — вызов от модели.
- [ ] Расширить `SModelMessage`: `m_tool_call_id` (для роли Tool), `m_tool_calls[]` (для Assistant).
- [ ] Добавить роль `EModelRole::Tool`.
- [ ] Расширить `SModelReply`: `m_tool_calls[]`, `m_finish_reason` (stop|tool_calls|length).
- [ ] Перегрузка `complete(messages, tools)` — с передачей схем.

### 1.2. `CModelOpenAICompat` (основной путь — локальная модель)
- [ ] Сериализовать `tools` в JSON-запрос (уже формат OpenAI).
- [ ] Парсить `choices[0].message.tool_calls` и `finish_reason`.
- [ ] Парсить и `content`, И `tool_calls` (модель может комментировать план).
- [ ] Роль `tool` + `tool_call_id` в запросе (уже поддерживает chat-template).
- [ ] SSE-поток: не терять `tool_calls` при стриминге (накапливать delta).
- [ ] Тест `model_test`: парсинг ответа с `tool_calls`.

### 1.3. `CModelClaude` (вторично, можно позже)
- [ ] Блоки `tool_use` / `tool_result`; маппинг `id`.
- [ ] Пометить как «не блокирует MVP».

---

## Фаза 2. Схемы инструментов (12 tools)

### 2.1. Реестр схем
- [ ] Функция `tool_schemas() -> std::vector<SModelTool>` в `CIndexTool`/`CToolRegistry`.
- [ ] По одной схеме на tool: `outline, callers, callees, refs, includes, overrides,
      dispatch, def, ancestors, derived, search, source`.
- [ ] Параметры: минимум `name` (цель); для `includes` — `path`.
- [ ] Enum-массивы для имён/описаний (Секция 3 rules.md) — без switch-case.

### 2.2. Формат результата для модели
- [ ] `format_tool_result(SToolResult) -> std::string` (компактный текст, Zero-Window).
- [ ] Переиспользовать логику `CEvidenceCollector` (срезы, лимиты).
- [ ] Явно ограничить длину (срезы), т.к. `n_ctx = 8192`.

---

## Фаза 3. Ядро цикла (ReAct)

### 3.1. `CReActAgent` (новый класс `src/core/`)
- [ ] `run(question, editor_ctx, out_answer, out_steps) -> bool`.
- [ ] Сборка стартового диалога: `[system + tool-defs, user(+editor hint)]`.
- [ ] Цикл:
  ```
  for step in 1..max_steps:
      reply = model.complete(messages, tools)
      if reply.has_tool_calls:
          for call in reply.tool_calls:
              result = registry.execute(parse(call), context)   // валидировать!
              messages += assistant(tool_calls) + tool(result)
          continue
      else:
          out_answer = reply.content; return true
  ```

### 3.2. Валидация вызовов от модели
- [ ] Парсить `arguments_json`; при ошибке — вернуть модели ошибку как tool-результат (не падать).
- [ ] Проверка имени tool по whitelist (`api_is_tool_command`/`parse_tool_name`).
- [ ] Проверка `allow_read` (§47) перед чтением.
- [ ] Guard: неизвестный tool → сообщение об ошибке в диалог.

### 3.3. Guardrails
- [ ] `max_steps` (напр. 8–12) — жёсткий лимит.
- [ ] Бюджет по времени/токенам; при исчерпании — «не удалось, вот что собрал».
- [ ] Срезание истории (компактификация tool-результатов) под 8192 ctx.
- [ ] Дедуп повторных одинаковых вызовов (анти-цикл) — как в B-002 (уточнения зациклились).

### 3.4. Прозрачность (§59)
- [ ] Каждый шаг логируется в stderr: `шаг N: tool(args) -> краткий итог`.
- [ ] `out_steps` — для `--verbose`/отображения в плагине.

---

## Фаза 4. Интеграция

### 4.1. Точка входа
- [ ] `CAskService`: при `m_react_loop` — делегировать в `CReActAgent`; иначе — текущий путь.
- [ ] Ответ — через существующий `out_answer` (уже сделан для протокола).
- [ ] Диагностика — в stderr (уже сделано).

### 4.2. Транспорт/CLI
- [ ] `serve`: работает без изменений (ask → loop → answer.text).
- [ ] CLI `ask`: флаг `--react` (опц.) для принудительного включения.
- [ ] `serve`: `params.react` (опц.) — переопределить флаг на один запрос.

### 4.3. Совместимость
- [ ] `Intent/pipeline/Evidence` остаются: **fast-path** для типовых запросов.
- [ ] Не удаляем и не ломаем; loop — параллельный режим.

---

## Фаза 5. Тесты

### 5.1. Юнит
- [ ] `react_test`: mock-модель возвращает последовательность tool_calls → ответ.
- [ ] Валидация: битый `arguments_json` → модель получает ошибку, loop продолжается.
- [ ] Guard: превышение `max_steps` → корректное завершение.
- [ ] Анти-цикл: повторный вызов → не бесконечность.

### 5.2. Интеграция (реальная модель, dogfood)
- [ ] Сценарий B-001 «Что делает этот класс?» — loop собирает outline+source.
- [ ] Сценарий B-002 «Все ли методы реализованы?» — `derived → outline/source`.
- [ ] Сценарий B-003 «как считается возвращаемое значение?» — `source` по курсору.
- [ ] Критерий: ответ + видимая цепочка шагов; завершение за ≤ max_steps.

### 5.3. Регрессия
- [ ] CTest 47/47 не деградирует.
- [ ] При `m_react_loop = NO` — прежнее поведение.

---

## Фаза 6. Документация

- [ ] `decisions.md`: ADR-23 «LLM-driven loop (ReAct) поверх детерминированных tools»;
      обоснование (эксперимент), границы, guardrails, fast-path vs loop.
- [ ] `plans/fcpp-agent-plugin-api.md`: отметить, что `ask` может выполняться loop'ом
      (внешний контракт не меняется).
- [ ] `requirements.md`: пометить §18/§30–31 как «fast-path, не единственный путь».
- [ ] Обновить `tasks/todo.md` (ссылка на этот план).

---

## Порядок выполнения (рекомендуемый)

1. **Фаза 1.1–1.2** (DTO + OpenAICompat) — без этого loop невозможен.
2. **Фаза 2** (схемы + формат результата).
3. **Фаза 3** (`CReActAgent` + guardrails).
4. **Фаза 4** (интеграция за флагом).
5. **Фаза 5** (тесты) — после каждого крупного шага.
6. **Фаза 6** (документация) — по ходу.

## Риски и ответы
- **Модель врёт в JSON** → детерминированный парсер + ошибка возвращается в диалог (не падаем).
- **Зацикливание** → `max_steps` + дедуп вызовов (урок B-002).
- **Раздувание контекста (8192)** → срезы tool-результатов, компактификация истории.
- **Ломаем существующее** → всё за флагом `m_react_loop`, дефолт NO.
- **Провайдер без native tool-calls** → fallback: просить JSON-строку, парсить самим.

## Критерий победы
На реальном вопросе (`entryToRow`-класса) агент **сам** строит цепочку
`search → source → (при необходимости ещё) → ответ`, без заранее заданного плана,
за ≤ max_steps, с видимыми шагами и корректным ответом.
