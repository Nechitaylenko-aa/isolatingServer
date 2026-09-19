# Слой автоматизации (2–4) — сводный референс

> Этот документ заменяет собой `automation_layer_context.md`, `runner_architecture.md`,
> `timeline_revision.md`, `automation_layer_summary.md`, `layer3_continue.md` — все они
> содержат устаревшие или уже пересмотренные решения по ходу сессий. Актуальное
> состояние кода на момент написания — **слой 3 финализирован**, `validate()` не даёт
> ложных срабатываний на нормальном band-control паттерне, backwash-цепочка замкнута
> и достижима из `SET_NORMAL`.
>
> Если через полгода что-то в коде разошлось с этим документом — верь коду, не
> документу, и обнови файл.

---

## 0. Зачем всё это

Единая модель данных проекта установки водоподготовки уже умеет: топологию (граф
`CCell`/`CCap`/`CConductor`), расчёт состава рабочего тела по графу, подбор
оборудования, сверку с нормативами. Не хватало одного — представления **алгоритма
управления как данных**, а не как императивного кода внутри методов компонентов.
Слой автоматизации закрывает это: из деклараций компонентов + фактов ТЗ он
механически строит дерево `Instrument`/`Actuator` и набор IF-THEN атомов
(`SAutomationAtom`), из которых в перспективе одинаково читаются симуляция, ST-код
для ПЛК и SCADA-теги.

## 1. Четыре слоя — коротко

| Слой | Что это | Где живёт | Статус |
|---|---|---|---|
| 1 | Топология графа (`CCell`/`CCap`/`CConductor`) | `core/` | готово, стабильно |
| 2 | Декларации возможностей компонента (`required_signals/commands`, `automation()`, `design_constraints()`) | виртуальные хуки на `NComponent` | готово |
| 3 | `bind()` — резолвер: превращает декларации слоя 2 + факты ТЗ в конкретные объекты слоя 4 | `IInstallationTemplate` и наследники | **финализирован в этой сессии** |
| 4 | Резолвленное дерево `Instrument*`/`Actuator*` + `SAtomSet`/`SAutomationAtom` — единственный источник для генерации артефактов | `CAutomationModel` | структура готова; codegen/симуляция — впереди |

Слой 3 — не персистентный класс, а orchestration-функция. Он не хранится, не
сериализуется — вызывается явно (`CAutomationModel::bind()`, отдельная GUI-команда,
не связана с `refresh_cell()`) и заново строит слой 4 с нуля (`clear()` в начале
`bind()`).

---

## 2. Карта файлов

```
NComponent.h/.cpp              — базовый класс компонента, 4 хука слоя 2, is_flow_boundary()
CAutomationAtoms.h             — ВСЕ структуры данных слоя 2 и слоя 4 (единый файл-контракт)
CAutomationModel.h/.cpp        — слой 4: хранилище Instrument/Actuator/SAtomSet, bind(), validate(), print()
IInstallationTemplate.h/.cpp   — базовый резолвер: traversal-примитивы, wire_boundary_patterns,
                                  wire_schedule_atoms, посредники к приватным сеттерам CAutomationModel
IWaterTreatmentTemplate.h/.cpp — специализация для водной ветки: backwash-паттерн
                                  (find_first_boundary_*, wire_filter_backwash_pattern,
                                  collectRecipes, emit_backwash_sequence, emit_stage_entry_snapshot)
CDrinkWaterInstallationTemplate.cpp — конкретный bind() для питьевой воды: собирает всё воедино
CLightFilter.h/.cpp            — пример компонента с двумя парами Cap (main + CT_ADDITIONAL)
CWaterCapacity.h/.cpp          — пример пассивного компонента-источника сигнала, is_flow_boundary()=true
CTwoWayValve.h/.cpp            — пример компонента-развилки (2 выхода), используется как flow_reverser
```

---

## 3. Ключевые структуры данных (`CAutomationAtoms.h`)

### 3.1 Слой 2 — декларативный (объявляется в хуках `NComponent`)

```
SSignalRole  { role, index, unit, subtype, prefix, cap_semantics, is_input, cap_index }
SCommandRole { role, index, unit, subtype, cap_semantics, is_input, cap_index, default_activate }
```

`cap_semantics` (`CS_FLOW_SPECIFIC` / `CS_COMPONENT_SCOPED`) — различает, важна ли
идентичность конкретного Cap для смысла сигнала (давление вход≠выход) или Cap —
просто физический якорь (перепад давления, команды промывки — свойства компонента
целиком). Cap-якорь физически есть всегда; разница только в семантике.

`default_activate` (bool на самой `SCommandRole`) — заменил собой раннюю (ошибочную)
попытку сделать `CR_DEFAULT_ACTIVATE` отдельным значением `ECommandRole`. Отдельное
значение enum было выпилено — оно порождало «фантомные» актуаторы в модели.

```
SAutomationSpec { trigger, algorithm, input_role, input_index, period_seconds,
                  duration_seconds, schedule_nature, output_roles, setpoint_source,
                  fixed_setpoint }
```

`schedule_nature` — критичное различение для `TT_SCHEDULE`:
- `SN_DUTY_CYCLE` — мигает актуатором внутри ТЕКУЩЕГО набора, топологию не меняет,
  генерируется механически (`wire_schedule_atoms`).
- `SN_PROCEDURE` — меняет топологию потока (например, backwash), требует своей
  цепочки `SAtomSet`, собирается доменной логикой резолвера, не механически.

### 3.2 Слой 4 — исполняемая форма (заполняется `bind()`)

```
SCondition   { signal_owner, role, index, op, threshold_source, fixed_threshold }
STimeTrigger { offset_from_entry_sec, period_sec, duration_sec }   // [offset, offset+duration)
STrigger     { kind (TRIG_EVENT|TRIG_TIME), event: SCondition, time: STimeTrigger }
SAtomCommand { target_owner, command: SCommandRole, activate }
SAutomationAtom { trigger: STrigger, actions: [SAtomCommand], origin: EAtomOrigin }
STransition  { to_set_id, trigger: STrigger }      // from_set_id неявен — лежит внутри своего SAtomSet
SAtomSet     { id, display_name, atoms: [...], transitions: [...] }
```

Атом всегда имеет форму **READ → COMPARE → COMMAND(s)** — без исключений; это и
объединяет будущую симуляцию и ST-кодоген вокруг одной статической структуры данных.

`EAtomOrigin` (`AO_BOUNDARY`/`AO_SCHEDULE`/`AO_MANUAL`) — не приоритет, а тег
происхождения для диагностики (кто на самом деле имеет право спорить с кем).

### 3.3 Каталог граничных паттернов

```
SBoundaryReactionRule { trigger_role, direction (UPSTREAM|DOWNSTREAM), reaction_role, activate, scope }
kBoundaryReactionCatalog — статическая таблица «что физически измеряется» → «чем это
                            гасится по умолчанию». Растёт вместе с каталогом ролей.
```

`scope` (`BRS_SUPERVISORY`/`BRS_NORMAL`) отличает аварийные интерлоки (HH/LL,
работают независимо от активного набора, живут в `m_supervisory_atoms`) от штатного
band-control (UPPER/MID/BOTT, живут внутри `SET_NORMAL`).

---

## 4. Dataflow: от топологии до атомов

```
┌─────────────────┐   NComponent-хуки    ┌──────────────────────┐
│  Слой 1 (граф)   │  ───────────────►   │ Слой 2 (декларации)  │
│  CCell/CCap/     │  required_signals()  │ required_commands()/ │
│  CConductor      │  required_commands() │ automation()/        │
│  E_CONTOUR_TYPE  │  automation()         │ design_constraints() │
└─────────────────┘  design_constraints() └──────────┬───────────┘
                                                       │ CAutomationCollector::collect(root)
                                                       ▼
                                       ┌───────────────────────────────┐
                                       │ SCollectedAutomationData      │
                                       │  .signals / .commands /       │
                                       │  .automations / .constraints  │
                                       └───────────────┬───────────────┘
                                                        │ IInstallationTemplate::bind(proj, data)
                                                        ▼
                          ┌──────────────────────────────────────────────────┐
                          │ Слой 3 — резолвер (CDrinkWaterInstallationTemplate)│
                          │  1. wire_boundary_patterns()  → кандидаты          │
                          │  2. wire_schedule_atoms()     → duty-cycle атомы   │
                          │  3. wire_filter_backwash_pattern() → кандидаты     │
                          │  4. collectRecipes()          → рецепты            │
                          │  5. emit_backwash_sequence()  → 3 набора+переходы  │
                          └───────────────────────┬────────────────────────────┘
                                                   │ add_instrument/add_actuator/
                                                   │ add_atom_set/add_atom_to_set/add_transition
                                                   ▼
                                   ┌───────────────────────────────┐
                                   │ Слой 4 — CAutomationModel      │
                                   │  m_instruments / m_actuators   │
                                   │  m_atom_sets / m_supervisory   │
                                   └───────┬───────────────┬───────┘
                                           │               │
                                  validate()          print() / (в перспективе)
                                  диагностика         ST-codegen, симуляция,
                                  конфликтов          SCADA-теги
```

Важно: сигнал/команда доставляются НЕ через цепочку Cap→Tube→Cap (это путь
рабочего тела), а через `CInfoBus`/адресацию на конкретный `NComponent*` —
кабель КИПиА физически независим от трубопровода. Traversal в резолвере ходит по
топологии Cap/Tube только чтобы НАЙТИ, кто физически стоит рядом — но сам атом
после этого хранит прямой указатель на компонент (`target_owner`, `signal_owner`),
не путь по графу.

---

## 5. Механизм контуров (`E_CONTOUR_TYPE`)

`CT_MAIN` / `CT_ADDITIONAL` на `CCap` — граница между главным технологическим
процессом (очистка воды, вдоль него стоят все компоненты) и накладывающимся
процессом (обратная промывка). Компоненты с backwash получают **вторую пару** Cap
(`add_ob(&ob, CT_ADDITIONAL)`), не переиспользуют основную пару. У ёмкостей — 3
входа (1 main + 2 additional про запас на будущее).

**Правило фильтрации — одно, но применяется в двух местах с разной ролью:**

- `find_components_upstream/downstream_matching` (используется и normal-, и
  backwash-резолвером через `find_components_*_with_command`) — фильтрует
  `CT_ADDITIONAL` **дважды на каждом шаге**: у соседней кепки на трубе И перед
  рекурсией внутрь пассивного компонента. Это гарантирует, что normal-резолвер не
  «утекает» через backwash-вход фильтра в чужой контур.
- `find_first_boundary_upstream/downstream` (backwash-резолвер, ищет
  `tank_before`/`tank_after`) — **не фильтрует контур внутри тела функции**. Контур
  учитывает вызывающий код, выбирая, с какого порта стартовать. Подтверждено на
  практике: главный контур — это и есть маршрут технологической очистки, все
  компоненты естественно стоят на нём; backwash подстраивается под него доп-контуром
  через клапан/трубы, поэтому поиск ёмкостей до/после фильтра идёт именно через
  `input(0)/output(0)` (main), а не через backwash-порт.

`detect_alternate_routes()`/DFS-детектор циклов — **не нужен** для этой задачи
(контур решает её раньше и декларативно). Функция оставлена в коде закомментированной
как отложенная идея — на случай общестанционного байпаса «от начала до конца
установки», где чистого контурного решения может не быть. Вызов из `bind()` убран.

---

## 6. `wire_boundary_patterns` — общий проход по графу

Не хук компонента, не зависит от `E_PROJECT_TYPE` — живёт один раз в
`IInstallationTemplate`. Ёмкость публикует `SR_LEVEL_HIGH`/`SR_LEVEL_LOW` и т.п. через
`required_signals()`, но *кого именно остановить* решает не она, а этот общий
проход по `kBoundaryReactionCatalog`.

Алгоритм на каждый `(сигнал, правило из каталога)`:
1. Выбрать стартовую кепку (`input(i)` для UPSTREAM, `output(i)` для DOWNSTREAM),
   пропустить, если её контур `CT_ADDITIONAL`.
2. `find_components_upstream/downstream_with_command(start, reaction_role)` —
   рекурсивный обход против/по потоку, пропускающий пассивные компоненты,
   останавливающийся на первом компоненте с нужной `ECommandRole`.
3. Каждая найденная пара → `SBoundaryInterlockCandidate` (никогда не пишется в
   модель напрямую — только кандидат).
4. `CDrinkWaterInstallationTemplate::bind()` материализует кандидатов в
   `SAutomationAtom`: `BRS_SUPERVISORY` → `add_supervisory_atom` (независимо от
   активного набора), `BRS_NORMAL` → `add_atom_to_set(normal_id, ...)`.

Если актуатор для сигнала не найден — текущая реализация: `set_error(ESE_ACTUATOR)`
на владельце сигнала и `continue` (пропускается именно эта связка сигнал↔правило,
не вся функция целиком — асимметрия upstream/downstream устранена).

---

## 7. `wire_schedule_atoms` — механическая часть расписаний

Обрабатывает **только** `TT_SCHEDULE` + `SN_DUTY_CYCLE` (например, ротация насосов
внутри станции — параметры считаны напрямую из `automation()` компонента, без
доменного вмешательства резолвера): генерирует пару атомов `ON`/`OFF` с
`TRIG_TIME`-окном `[offset, offset+duration)` внутри периода.

Всё, что `SN_PROCEDURE` (например, «раз в 3 суток запустить backwash»), сознательно
**не** трогается здесь — складывается в возвращаемый список
`undeclared_procedures`, который `bind()` обязан подхватить сам (см. §8) — это и
есть точка стыковки механического расписания с доменной процедурной логикой.

---

## 8. Backwash-цепочка — сквозной сценарий

### 8.1 Поиск участников (traversal, автоматически)

`wire_filter_backwash_pattern(proj, data)` — для каждого фильтра (по типу компонента,
без дублей на 2 клапана одного фильтра):
```
filter               — сам компонент из data.commands
tank_before/after     — find_first_boundary_upstream/downstream от input(0)/output(0)
recirculation_pump    — find_components_downstream_matching(tank_after, is_pump)
flow_reverser         — find_components_downstream_matching(tank_after, is_two_way_valve)
```
Результат — `SBackwashCandidate`. Обязателен null-guard: если топология неполная
(нет ёмкости после фильтра, нет насоса и т.п.) — кандидат пропускается целиком, а не
падает при разыменовании.

### 8.2 Рецепт (доменное решение инженера, не выводится traversal'ом)

`collectRecipes(candidates)` заполняет `SBackwashRecipe` — какие клапаны/насосы в
каком положении на каждой стадии (`prep_overrides`/`backwash_overrides`/
`restore_overrides`), плюс длительности (`prep_duration_sec`, `water_pulse_sec` и
т.д.). Это **не автогенерируется** — инженер вписывает руками для каждой конкретной
установки, потому что здесь заканчивается то, что можно вывести из топологии, и
начинается технологическое know-how.

### 8.3 Материализация (`emit_backwash_sequence(model, last_id, data, recipe, candidate, trigger_period_sec)`)

Создаёт 3 набора (`PREP`/`BACKWASH`/`RESTORE`) поверх `last_id` (обычно `SET_NORMAL`).
Каждый набор при входе получает **полный снапшот** состояния всех актуаторов
(`emit_stage_entry_snapshot` проходит по ВСЕМ `data.commands`, берёт override из
рецепта либо `default_activate`) — не дельту от предыдущего состояния. Это
гарантирует «всё лишнее выключено», не полагаясь на то, что было активно раньше.

**Диаграмма состояний (state machine), 4 набора:**

```
                    ┌─────────────────────────────────────────┐
                    │                                           │
                    ▼                                           │
   ┌───────────────────────┐  after trigger_period_sec   ┌──────────────┐
   │      SET_NORMAL        │ ──────────────────────────► │ BACKWASH_PREP │
   │  (band-control по      │                              │  (снапшот:    │
   │   уровням двух ёмкостей│                              │   всё стоп,   │
   │   + supervisory HH/LL) │                              │   реверс открыт)│
   └───────────────────────┘                              └──────┬───────┘
                    ▲                                             │ after prep_duration_sec
                    │                                             ▼
       tank_after.BOTT == 1                              ┌───────────────┐
                    │                                    │   BACKWASH     │
            ┌───────────────┐                            │ (насос-рецирк. │
            │    RESTORE     │◄───────────────────────────│  качает,       │
            │ (снапшот:      │  tank_after.UPPER == 1     │  клапан-реверс │
            │  всё стоп,     │                            │  открыт)       │
            │  реверс закрыт)│                            └───────────────┘
            └───────────────┘
```

Переход `SET_NORMAL → BACKWASH_PREP` берёт период из `SN_PROCEDURE`-спеки фильтра
(найденной в `undeclared_procedures` из §7 по `owner == candidate.filter`) — не
хардкод. `PREP → BACKWASH` — через `recipe.prep_duration_sec`. `BACKWASH → RESTORE`
и `RESTORE → SET_NORMAL` — событийные (`TRIG_EVENT`, `CMP_EQ`, `fixed_threshold=1`,
т.к. это дискретные уровневые сигналы, конвенция «1 = истина» на всей модели).

---

## 9. `validate()` — что реально проверяется

Группирует все атомы всех наборов по `(target_owner, command.role, command.index)` —
т.е. по конкретному актуатору. Внутри группы:

- **Если все атомы `TRIG_TIME`** — проверяется реальное пересечение временных окон
  с противоположными командами (`activate` разный). Разные периоды → issue
  («overlap not provably excluded»); одинаковый период, окна пересекаются и команды
  противоположны → issue («overlapping time windows»).
- **Если хотя бы один атом `TRIG_EVENT`** — группа пропускается без issue. Разные
  `signal_owner` на одном актуаторе (например, насос между двумя ёмкостями слушает
  обе стороны) — это легитимный паттерн, не конфликт: порядок атомов внутри
  `SAtomSet::atoms` уже разрешает коллизию так же, как порядок рунгов в лестничной
  логике ПЛК — последний по списку физически побеждает при совпадении по скану.
  Это никогда не значит одновременность на одном физическом уровне: состояния
  одной ёмкости (`UPPER`/`MID`/`BOTT`) взаимоисключающи по определению.

Единственная категория, которую `validate()` реально обязан ловить — пересечение
`TRIG_TIME`-окон с противоположными командами. Всё остальное — не относится к его
задаче.

---

## 10. Осознанно отложено (не блокер финализации)

- **Воздуходувка / duty-cycle воздух-вода внутри `BACKWASH`** — компонента нет ни в
  одном текущем проекте; код (`r.blower`-ветка) корректно неактивен, ничего не
  притворяется рабочим. Когда появится компонент-воздуходувка — добавить predicate
  в traversal (`pred_blower`, по аналогии с `pred_pump`), завести роль
  `CR_BLOWER_START_STOP` (уже есть в enum) в `SBackwashCandidate`/рецепте.
- **`SBackwashRecipe` несёт дублирующие поля** (`filter/tank_before/tank_after/...`),
  раз `emit_backwash_sequence` теперь получает `SBackwashCandidate` отдельным
  параметром. Не мешает работать — гигиена на будущее.
- **`CSimulationShadow`** — параллельный рантайм-класс (аналог `IShadow`, но
  стейтфул), нужен для того, чтобы `kStateDerivedSignalRoles`
  (`SR_LEVEL_HH/LL`, `SR_PUMP_STATUS`) реально считались в режиме SIMULATION —
  сейчас структурно всё готово (роли объявлены, атомы собраны), но
  `CPhysicalInstrument::value()` для этих ролей ничего не читает без него.
- **ST-кодоген** — не начат. Условие готовности («минимум два непохожих сквозных
  сценария») выполнено (band-control normal + backwash-процедура), codegen должен
  читать только `m_atom_sets`/transitions/резолвленные setpoint'ы — никогда
  `Instrument::value()` или `CSimulationShadow` напрямую (иначе смешается
  design-time и runtime).
- **PLC-адресация** (`%DIX.X`/`%AIX.X`) — заглушки, реальный модуль адресации не
  спроектирован.
- **Второй/третий фильтр с backwash** — `base_id`/`last_id` для
  `emit_backwash_sequence` назначается вручную вызывающим кодом (осознанно, без
  автогенератора id). Не забыть уникальный диапазон id при добавлении второго
  фильтра.
- **`CPumpStation` → `CR_PUMP_GROUP_STEP_UP`** — если ротация дежурный/резервный
  насос внутри станции реализуется через `automation()`, она обязана использовать
  отдельную от `CR_PUMP_START_STOP` роль (управление станцией снаружи не должно
  видеть внутреннюю ротацию) — сверить при реализации `CPumpStation::automation()`.

---

## 11. Пример на реальной топологии (использован для финализации)

```
Intake → НС4(id4, насос) → Е7(id7, ёмкость) → НС14(id14, насос) → Е22(id22, ёмкость)
       → НС29(id29, насос) → ФО17(id17, фильтр, backwash через КО76/id32) → outlet
```

`model->print()` после финализации показывает:
- `SET_NORMAL` — band-control по 5 уровням на Е7 и Е22 (UPPER/MID/BOTT на каждую,
  насосы НС4/НС14/НС29 между ними), 4 supervisory-атома (HH/LL на обеих ёмкостях).
- Переход `SET_NORMAL → BACKWASH_PREP` — по периоду из `ФО17::automation()`
  (`TT_SCHEDULE`/`SN_PROCEDURE`, "раз в N суток").
- `BACKWASH_PREP → BACKWASH → RESTORE → SET_NORMAL` — с реальными длительностями
  рецепта и событийными условиями по уровню Е22 (`tank_after`).
- `validate()` (после фикса §9) — issues пусты на этой топологии.

Это и есть рабочий эталон — при любом расхождении сверяйся с логом такого прогона,
а не с этим документом (документ описывает механизм, лог — конкретное состояние).
