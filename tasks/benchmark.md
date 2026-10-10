# fcpp-agent: Живой benchmark классификации (§71–72)

Каждый обнаруженный реальный промах становится новым кейсом (процесс §72):
`bug → reproduce → benchmark case → fix → regression test`.

Структура записи (§71): фраза, курсор, проект-фикстура, ожидаемые
`Area / Subject / Relation / Projection / Effect`, ожидаемые tools,
требуемые свойства ответа.

---

## B-001 — «Что делает этот класс?» (курсор в методе)

```text
Input:       "Что делает этот класс?"
Cursor:      dogfood/src/model/shape.hpp:25:15   (внутри Shape::area)
Fixture:     dogfood
Expected:
    Area       = UNDERSTAND
    Subject    = CLASS dogfood::model::Shape
    Relation   = SELF
    Projection = EXPLANATION
    Effect     = READ_ONLY
    Tools      = outline, source
Answer:      описание класса Shape (абстрактная фигура, virtual dtor, area/name)
Status:      PASS (после ADR-13/ADR-14)
```

Регресс-суть: курсор не должен затирать вид от фразы; имя — ближайшая
охватывающая сущность вида (ADR-13).

---

## B-002 — «Все ли методы у наследников реализованы?» (агрегат по иерархии)

```text
Input:       "Все ли методы у наследников реализованы или есть заглушки?"
Cursor:      dogfood/src/model/circle.hpp:26:14   (Circle::area)
Fixture:     dogfood
Observed (до декомпозиции, BAD):
    Area       = VERIFY            (ожидалось UNDERSTAND — §19: VERIFY = build/tests)
    Subject    = METHOD dogfood::model::Circle::area  (сужение: нужен агрегат по наследникам)
    Relation   = REFERENCES        (ожидалось OVERRIDES)
    Projection = EXPLANATION
    Outcome    = NOT_UNDERSTOOD: no route for intent

Observed (после декомпозиции ADR-17, всё ещё BAD):
    Декомпозиция: 3 подвопроса (привязка к Circle — неверный якорь)
      - Какие методы определены в классе Circle?
      - Какие методы переопределены в наследниках класса Circle?
      - Какие методы в классе Circle имеют реализацию по умолчанию?
    шаг 1: UNDERSTAND CLASS Circle / SELF / SIGNATURE      -> CLARIFY (Circle vs Circle::Circle)
    шаг 2: DESIGN CLASS Circle / OVERRIDES / SIGNATURE     -> NOT_UNDERSTOOD: no route
    шаг 3: DESIGN METHOD Circle / REFERENCES / SIGNATURE   -> NOT_UNDERSTOOD: no route

Expected:
    Area       = UNDERSTAND
    Subject    = TYPE Shape (anchor = базовый класс; обход Derived: Circle, Rectangle)
    Relation   = OVERRIDES
    Projection = SOURCE (тела) для проверки заглушек
    Effect     = READ_ONLY
    Tools      = derived, overrides, dispatch, source
Answer:      сводка: переопределены ли area()/name() у всех наследников; есть ли заглушки

Observed (после ADR-18/ADR-19, PASS):
    Декомпозиция: 3 подвопроса
      - Какие методы определены в классе Shape?
      - Какие методы переопределены в наследниках класса Shape?
      - Какие методы в наследниках класса Shape не переопределены?
    Понял так: UNDERSTAND CLASS Shape / REFERENCES / SOURCE
    Понял так: UNDERSTAND CLASS Shape / OVERRIDES / SIGNATURE
    Понял так: UNDERSTAND METHOD Shape / OVERRIDES / SIGNATURE
    Вердикт таблица:
      | Наследник | Метод | Реализация  |
      | Circle    | area  | implemented |
      | Circle    | name  | implemented |
      | Rectangle | area  | implemented |
      | Rectangle | name  | implemented |
    Все методы у наследников реализованы.

Status:      PASS
```

Найденные слои проблемы (ADR-18) — устранены:
1. Якорь: `hierarchy_anchor` через `ancestors` → `Shape` ✔.
2. Классификация Фазы B: промпт + детерминированная пост-коррекция (`UNDERSTAND`/`OVERRIDES`) ✔.
3. Маршруты под тела/сигнатуры добавлены ✔.
4. Фильтр по `Subject.kind` в резолвере — ложный `CLARIFY` снят ✔.
5. Финал: таблица-вердикт «наследник × метод» ✔.
6. Fan-out (ADR-19, path A): `outline(Shape)` → методы → `dispatch_targets` → `source`; лимиты 10/20 ✔.
7. Составной режим: подвопросы самодостаточны; якорь подставляется при пустом имени ✔.

---

## B-003 — «как считается возвращаемое значение?» (курсор на методе, атомарный)

```text
Input:       "как считается возвращаемое значение?"
Cursor:      dogfood/src/model/rectangle.hpp:27:14   (сигнатура Rectangle::area)
Fixture:     dogfood
Observed (BAD, до B2):
    Понял так: UNDERSTAND METHOD Shape / SELF / SOURCE       (уход к базе)
    ответ про Shape
Expected:
    Area       = UNDERSTAND
    Subject    = METHOD dogfood::model::Rectangle::area
    Effect     = READ_ONLY
    Tools      = source
Answer:      как вычисляется тело Rectangle::area (m_width * m_height)
Status:      PASS (после B2/ADR-20)
```

Observed (PASS):
```
Понял так: UNDERSTAND METHOD dogfood::model::Rectangle::area / SELF / SOURCE
Возвращаемое значение функции `area()` - это произведение `m_width` и `m_height`.
```

Причина прежнего промаха: lite не видела символ под курсором (`name=""`), а якорь
иерархии безусловно подставлял базовый класс (`Shape`). Фикс (ADR-20, B2): символ
под курсором резолвится через `IClang` и передаётся lite как `[cursor symbol: NAME (KIND)]`;
применимость решает сама модель. В составном режиме курсорный хинт не подаётся, чтобы
не вытеснять подвопросы.

---

## Процесс использования

1. Прогнать сценарий (`ask` с курсором на фикстуре `dogfood`).
2. Сравнить фактическую интерпретацию («Понял так: …») с `Expected`.
3. При промахе — добавить/обновить кейс; после фикса — пометить `PASS`.
