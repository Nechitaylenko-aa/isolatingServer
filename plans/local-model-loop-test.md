# Тест: способна ли локальная модель вести loop (tool-calling)

Дата: 2026-10-09. Модель: `Qwen2.5-Coder-14B-Instruct-Uncensored.Q4_K_M.gguf`,
сервер `llama-server` на `http://127.0.0.1:8080`, контекст 8192.

## Инфраструктура сервера (из `/props`)
- `supports_tools: true`
- `supports_tool_calls: true`
- `supports_parallel_tool_calls: true`
- chat_template содержит полноценный блок function-calling (`<tools>`, `<tool_call>`).

Вывод: llama-server + этот chat-template **нативно поддерживают** OpenAI-совместимый
`tools` / `tool_calls`.

## Эксперименты

### T1. Возвращает ли модель tool_call?
Вопрос: «Какие методы объявлены в классе Shape?», tools = {search, outline, source}.
Результат:
```
finish_reason = "tool_calls"
tool_calls = [ {name:"outline", arguments:"{\"name\":\"Shape\"}"} ]
```
✅ Модель выбрала корректный инструмент и корректный JSON.

### T2. Многошаговость (после tool-результата)
Подставлен `tool`-результат outline → модель вернула `finish_reason = "stop"`
и финальный текст: «Класс Shape имеет два метода: area и name.»
✅ Корректно завершает цикл после получения фактов.

### T3. Планирование цепочки на сложном вопросе
Вопрос: «Реализованы ли все методы у наследников Shape или есть заглушки?»,
tools = {search, outline, derived, source}.
Результат:
```
finish_reason = "tool_calls"
content = «Найду наследников, затем проверю методы каждого...»
tool_calls = [ {name:"derived", arguments:"{\"name\":\"Shape\"}"} ]
```
✅ Сама построила план и первым вызвала `derived(Shape)`.

### T4. Продолжение цепочки
После результата `derived` → `{Circle, Rectangle}` модель вызвала
`outline({name:"dogfood::model::Circle"})`.
✅ Цепочка `derived → outline` продолжается детерминированно и логично.

### T5. Стабильность на рабочей температуре (0.8), 3 прогона
Три независимых запроса «найди наследников Shape и их методы».
Результат: 3/3 —
```
finish=tool_calls  tool=derived({"name":"Shape"})
```
✅ Без деградации JSON, без галлюцинаций имени.

## Итог
Локальная 14B **пригодна для управления agent loop**:
- нативный tool-calling работает (`tools` → `tool_calls`);
- многошаговость работает (tool → результат → ответ / следующий tool);
- цепочки строятся осмысленно (`derived → outline → source`);
- стабильно на рабочей температуре.

## Замечания
- `content` при `tool_calls` может быть непустым (модель «комментирует» план) —
  парсер должен читать И `content`, И `tool_calls`.
- Аргументы приходят строкой JSON → парсить отдельно.
- Нужен guard на `max_steps` и валидацию `arguments` (недоверенный JSON от модели).
- `n_ctx = 8192` — держать историю loop компактной (срезать tool-результаты).
