# Путеводитель: где что регистрируется в слое автоматизации/симуляции

## 1. Сигналы (Instrument) — три независимых пути регистрации

| Тип роли | Кто регистрирует | Метод | Когда |
|---|---|---|---|
| Flow-derived (`SR_PRESSURE_PV`, `SR_DP_FILTER`) | `IInstallationTemplate::bind()`, цикл по `candidates` | `add_instrument(model, signal_anchor, role)` → `CPhysicalInstrument` | если `!is_state_derived_role()` |
| State-derived в условиях атомов (`SR_LEVEL_*`, `SR_PUMP_STATUS`) | `IInstallationTemplate::add_atom_to_set/add_supervisory_atom/add_transition` (медиаторы) | `ensure_instruments_for_trigger()` → `add_internal_instrument()` → `CInternalInstrument` | автоматически, при любом `STrigger` с `SCondition`, независимо от того, кто сгенерил атом |
| State-derived в переменных (`moto_hours.condition_role`) | `IInstallationTemplate::add_internal_variable()` | тот же `add_internal_instrument()`, отдельный вызов | не зависит от атомов — `SVariableBehaviorSpec` не содержит `STrigger` |

**Итог:** state-derived роль регистрируется НЕ там, где её физически используют (атом/переменная), а в **медиаторах** `IInstallationTemplate` — единственном месте, через которое обязан пройти любой атом/переход/переменная перед попаданием в `CAutomationModel`.

## 2. Значения сигналов (runtime) — тоже два разных механизма

- **`CInternalInstrument::value()`** читает НЕ тень напрямую, а `owner->read_state_signal(role, index)` — push-кэш на `NComponent` (`m_state_signals`).
- **Кто пишет в этот кэш:**
    - `CSimWaterCapacity::tick()` — пишет `SR_LEVEL_*` напрямую на себя, вручную, каждый тик (пример из чата).
    - Для `SR_PUMP_STATUS`/`SR_PUMP_STATION_FAIL` — **генерически**, в `CSimulationRunner`, через новый хук `ISimulationShadow::readable_state_roles()`: раннер каждый тик проходит по `m_simShadowsMap`, берёт `shadow->read(role, idx)` для каждой задекларированной роли и пишет в `owner->set_state_signal(...)`.

**Важно:** регистрация `Instrument` (раздел 1) и синхронизация его значения (раздел 2) — **независимые** требования. Забыть можно любое из двух по отдельности (так и происходило).

## 3. Кодогенная тень (`ICodegenShadow`) — где реально вызывается

```cpp
// CDrinkWaterInstallationTemplate::bind(), ВНУТРИ цикла по candidates
auto shadow = CCodegenShadowsManager::codegen_shadow_instance(c.target_owner);
```

Это **единственное** место во всём проекте, где кодогенная тень (`CShadowPumpStation` и т.п.) вообще создаётся/вызывается. Если компонент не попал в `candidates` (результат `wire_boundary_patterns()`) — его `build_internal_topology()`/ротационные атомы никогда не строятся через этот путь.

**ВС98 (воздуходувка) сюда не попадает** — она не участвует в обычном течении процесса (нет боундари-интерлока уровень→воздуходувка), значит `CShadowAirBlower`/аналог `build_rotation_atoms()` для неё **не вызывается вообще**. Атомы для ВС98 (чередование, клапаны) приходят из **другого, параллельного пути** — `IWaterTreatmentTemplate::emit_alternation_atoms()`/`make_valve_atoms()` (backwash-специфика), не связанного с `candidates`/кодогенной тенью вообще.

Отсюда и была путаница в сессии: искали "чью-то ротацию с idx=1" в кодогене — а нашли в `variable_behaviors()` (moto_hours), которая обходит **все** компоненты безусловно, вне `candidates`.

## 4. Три независимых обхода всех компонентов в `bind()` — не путать друг с другом

```cpp
bool CDrinkWaterInstallationTemplate::bind(...)
{
    auto candidates = wire_boundary_patterns(proj, data);   // (A) не все компоненты — только боундари-цепочки

    for (auto &c : candidates) { ... codegen_shadow_instance(c.target_owner) ... }  // кодоген — только для (A)

    // (B) БЕЗУСЛОВНО все компоненты проекта — moto_hours и т.п.
    for (auto *item : *proj->project_cell()->get_components())
        for (auto vb : comp->variable_behaviors())
            add_internal_variable(model, vb);

    wire_schedule_atoms(...);            // (C) TT_SCHEDULE — тоже не через candidates
    wire_filter_backwash_pattern(...);   // (D) backwash — тоже не через candidates
}
```

Каждый из четырёх путей (A/B/C/D) — своя логика обхода компонентов, свои условия попадания. Компонент может участвовать в (B)/(C)/(D), не участвуя в (A) — это нормально и именно так устроена ВС98.

## 5. Хронология находок этой сессии (для памяти)

1. `CSimulationRunner::process_command()` не форвардил `CR_PUMP_START_STOP` в тень → откачено, оказалось несущественным на фоне следующих находок.
2. `CInternalInstrument` не был зарегистрирован для `SR_PUMP_STATUS` нигде → фикс через `ensure_instruments_for_trigger`.
3. `CInternalInstrument::value()` читает кэш `NComponent`, а не тень — сам кэш никто не обновлял → фикс через `readable_state_roles()` + генерическая синхронизация в раннере (не через ручные push в каждой точке мутации тени).
4. `CSimAirBlower` не переопределял `readable_state_roles()` (скопирован с `CSimPumpStation` до появления этого хука).
5. `rebuild_internal_topology()` был закомментирован в конструкторе `CPumpStation` → перенесён в обход компонентов при `bind()` (топология пересобирается с нуля при каждом прогоне по кнопке, накопленное состояние между прогонами не нужно).
6. `moto_hours`-путь (`add_internal_variable`) не регистрировал инструмент для своего же `condition_role` → отдельный фикс, независимый от атомного пути.
7. `CSimWaterCapacity::tick()` — `rate` без `dt`/`m_volume`, калибровочная константа осушала `tank_after` быстрее цикла чередования → подобрано вручную (`0.01 → 0.001`), реальная формула — отложенный технический долг.