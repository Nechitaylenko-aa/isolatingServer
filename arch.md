# Архитектура конвейера задачи (Primitive → Detail → CursorContext → Relevance → Action)

## Проблема, которую решает этот документ

До этой переделки `UnderstandHandler` и `GroundService` пытались угадать,
что делать, разбирая один и тот же JSON в нескольких местах разными способами.
Каждый новый кейс (чисто виртуальный метод, заголовок не в
`compile_commands.json`, pimpl-реализация, "можно ли заменить два метода
одним" без правок) требовал нового `if` где-то внутри уже существующей ветки,
и было неизвестно заранее, в какую ветку он должен попасть. Это и была "дыра",
которую латали трижды подряд.

Вместо одного `enum class Primitive` с добавками, конвейер разбит на **5
независимых слоёв**. Каждый слой отвечает ровно на один вопрос и ничего не
знает про слои выше по стеку вызовов, кроме своего прямого входа.

## Слои конвейера

```
Слой 0: CursorBinding   — нужен ли курсор этой задаче вообще
Слой 1: Primitive+Detail — что хочет юзер (область + детализация)
Слой 2: CursorContext    — где мы физически находимся в коде
Слой 3: Relevance        — относится ли место (слой 2) к задаче (слой 1)
Слой 4: Action           — что конкретно исполнить
```

Важное свойство: **слой 2 (CursorContext) вычисляется независимо от слоя 1**.
Это чистый факт о коде под курсором (или его отсутствии), не зависящий от
того, что сформулировал юзер. Слои 1 и 2 сходятся только в слое 3.

---

### Слой 0 — `CursorBinding`

Отвечает на вопрос: **относится ли goal_text к конкретному месту в коде, или
это общий вопрос/задача без привязки к курсору?**

```cpp
enum class CursorBinding {
    NOT_NEEDED,       // задача не про конкретное место: архитектура, общий
                       // вопрос о подсистеме, создание нового файла с нуля
    NEEDED_PRESENT,    // задача привязана к месту, и курсор (file/line/col) есть
    NEEDED_MISSING,    // задача привязана к месту, но курсор пуст/невалиден
};
```

**Кто вычисляет:** light-модель, тем же вызовом, что и `Primitive`/`Detail`
(см. слой 1) — отдельного похода в модель не требуется. Формулировок, которые
означают "это не про конкретное место" ("спроектируй архитектуру", "как
работает вся подсистема X"), слишком много, чтобы перечислять ключевыми
словами — этим надёжно справляется модель, а не regex.

**Зачем:** если `NOT_NEEDED` — слои 2 и 3 пропускаются целиком, `Ground`
(дорогой libclang-парсинг) вообще не вызывается. Если `NEEDED_MISSING` —
сразу `needs_input`/`error`, не доходя до Ground — экономим заведомо
бессмысленный парсинг.

---

### Слой 1 — `Primitive` + `Detail`

`Primitive` (уже существовал) — область задачи: `UNDERSTAND`, `SANDBOX_FIX`,
`QUARRY_DESIGN`, `TEST_GEN`, `EXPERIMENT`, `INFRA`.

`Detail` — новый enum, детализация **намерения внутри Primitive**. Вычисляется
тем же вызовом модели, одним дополнительным полем JSON. Полный список (см.
`task_classification.h`) разбит по Primitive:

- **UNDERSTAND** — графовые запросы (`CALL_CHAIN`, `CALL_GRAPH_SUBTREE`,
  `DEPENDENCY_MAP`, `DATA_FLOW`, `TYPE_USAGE`, `LIFECYCLE_TRACE`),
  структурные (`FIND_IMPLEMENTATIONS`, `ANALYZE_INTERFACE`, `COMPARE_IMPLS`,
  `DIFF_INTERFACE_IMPL`), виртуальные симуляции без правок
  (`VIRTUAL_REFACTOR_SIM`, `FEASIBILITY_CHECK` — "можно ли заменить эти два
  метода одним", "можно ли перенести метод в другой класс" — это **анализ
  смысла**, не правка), качество/долг (`CODE_SMELL`, `DUPLICATE_FIND`,
  `COMPLEXITY_HOTSPOT`, `TODO_SCAN`), документация (`DOC_GEN`, `DOC_CHECK`,
  `DOC_SUMMARY`), безопасность (`SECURITY_AUDIT`, `INPUT_VALIDATION`,
  `THREADING_SAFE`), модернизация (`MODERNIZE`, `API_MIGRATION`,
  `PORT_PLATFORM`), общий вопрос (`GENERAL_QUESTION`, `SIMPLE_READ`).
- **SANDBOX_FIX** — локальная правка (`LOCAL_FIX`), каскадные
  (`RENAME`, `SIGNATURE_CHANGE`, `ADD_OVERRIDE`), структурный рефакторинг
  (`EXTRACT_METHOD`, `INLINE_METHOD`, `EXTRACT_CLASS`, `MOVE_METHOD`),
  удаление (`DEAD_CODE_REMOVAL`, `REMOVE_UNUSED_INCL`), добавление
  (`ADD_METHOD`, `ADD_CLASS`).
- **QUARRY_DESIGN** — `NEW_INTERFACE`, `API_DESIGN`, `APPLY_PATTERN`,
  `SPLIT_CLASS`, `DESIGN_HIERARCHY`.
- **TEST_GEN** — `UNIT_TEST`, `INTEGRATION_TEST`, `GEN_MOCKS`,
  `COVERAGE_GAP`.
- **EXPERIMENT** — `PROTOTYPE`, `BENCHMARK`, `TRY_ALTERNATIVE`.
- **INFRA** — `BUILD_FIX`, `BUILD_CONFIG`, `DEPS_UPDATE`, `CI_PIPELINE`.
- **META** — `EXCEEDS_LOCAL_AI` — задача признана выходящей за рамки того,
  что локальная модель способна надёжно решить (см. раздел "Категория C"
  ниже).

Полный список зафиксирован целиком даже там, где Action ещё не реализован
(см. ниже) — чтобы при появлении новой формулировки от юзера она попадала в
существующий `Detail`, а не создавала новую незапланированную ветку.

---

### Слой 2 — `CursorContext`

Отвечает на вопрос: **что физически находится под курсором**, вычисляется
**детерминированно** (libclang AST, с текстовым fallback если TU не парсится),
полностью независимо от того, что просил юзер.

```cpp
enum class CursorContext {
    NO_CONTEXT,                  // файл не открыт / курсор нулевой / невалиден
    PURE_VIRTUAL_METHOD,         // метод = 0 в интерфейсе
    VIRTUAL_METHOD_WITH_BODY,    // virtual с реализацией в базовом классе
    CONCRETE_METHOD_IMPL,        // обычная реализация метода (override или нет)
    NON_VIRTUAL_METHOD,          // метод класса, не помеченный virtual
    PIMPL_IMPL,                  // реализация в .cpp (Impl-класс pimpl-паттерна)
    INTERFACE_CLASS_DECL,        // курсор на объявлении класса (class X { ... })
    CC_FOREST,                   // между классами в .h, или до/после класса (вне тела)
    PLAIN_HEADER,                // .h с enum/struct/typedef, не методы класса
    FREE_FUNCTION,               // свободная функция, не метод
};
```

Этот слой решает исходную "дыру": раньше `buildGroundLight` пыталась угадать
по одной попытке `queryLocateSymbol`, и если она падала с `parse_failed`
(типичный случай — заголовок не входит как отдельный TU в
`compile_commands.json`), не было единого места, решающего "что делать
дальше". Теперь `CursorContext` — результат явного зондирования (может
потребовать несколько попыток: `queryLocateSymbol`, при неудаче — текстовый
разбор файла), и из него *всегда* выходит одно из перечисленных значений, а
не ошибка.

---

### Слой 3 — `Relevance`

Отвечает на вопрос: **относится ли то, что мы нашли под курсором (слой 2), к
тому, что просил юзер (слой 1)?**

```cpp
enum class Relevance {
    ENTRY_POINT_EXACT,    // курсор точно на том, что просили
    ENTRY_POINT_NEARBY,   // курсор в правильном файле/классе, но не на точной строке
    UNRELATED,            // курсор вообще не похож на то, что описывает задача
    AMBIGUOUS,             // не можем решить без юзера
};


Вычисляется детерминированной таблицей computeRelevance(Detail, CursorContext) (см. task_classification.cpp): например, если Detail::GENERAL_QUESTION — курсор неважен (ENTRY_POINT_NEARBY независимо от контекста), если Detail требует метод, а CursorContext — метод, это ENTRY_POINT_EXACT, а если CursorContext == NO_CONTEXT — всегда UNRELATED.

Слой 4 — Action
Финальный выбор — что конкретно исполнить. Выбирается таблицей selectAction(primitiveStr, Detail, CursorContext), учитывая Relevance как модификатор (если UNRELATED/AMBIGUOUS — форсим NEEDS_INPUT_RELEVANCE независимо от остального).

```cpp
enum class Action {
    // Категория A: реализовано прямо сейчас
    QUERY_LOCATE_ONLY,           // показать сигнатуру метода
    QUERY_FIND_IMPLEMENTORS,     // queryFindImplementors — поиск реализаций
                                   // через наследование (для заголовков не в
                                   // compile_commands.json / pure virtual)
    QUERY_VIRTUAL_OVERRIDES,     // queryVirtualOverrides — поиск overrides
                                   // через USR (для .cpp с виртуальным методом)

    // Категория B: архитектурно заложено, query ещё не написан
    NOT_IMPLEMENTED_YET,         // Detail признан enum'ом, но механизм/query
                                   // для него ещё не реализован — честная
                                   // ошибка вместо тихого провала или угадывания

    // Категория C: превышает возможности локальной модели
    DELEGATE_TO_MAIN,             // отдать main-модели на reasoning
                                   // (например VIRTUAL
```
