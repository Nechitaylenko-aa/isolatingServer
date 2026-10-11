# fcpp-agent: Глобальный Бэклог

Порядок реализации (ADR-12): детерминизм вперёд, LLM — последним.
CTest: число тестов отслеживается в последнем CI-run (сборка `-Wall -Wextra -Werror`).

> **НОВОЕ направление (план):** LLM-driven loop (ReAct) поверх детерминированных tools.
> Эксперимент подтвердил: локальная `Qwen2.5-Coder-14B` нативно умеет `tools`/`tool_calls`.
> Детальный план: [`plans/react-loop-todo.md`](../plans/react-loop-todo.md);
> отчёт эксперимента: [`plans/local-model-loop-test.md`](../plans/local-model-loop-test.md).

## [x] Этап 0: Архитектурное проектирование и устранение неопределенностей
* [x] ADR Блоки 1–3 в `decisions.md`; ADR-11 (IClang/API libclang); ADR-12 (политика схемы и порядок).
* [x] Базовые абстракции: `IIndex`, `ILLMClient`, `IExecutor`, `IClang`, `ITool`, `IModelApi`, `IAgentApi`.

## [x] Этап 1: Скелет проекта и CLI Инфраструктура
* [x] C++20 + CMake + Ninja; FetchContent: nlohmann/json, sqlite3; hard-require libclang 22 и libcurl.
* [x] Конфигурация rc.conf (`CConfigManager`): bootstrap + load/validate + **merge недостающих ключей** + `reload_config()`.
* [x] Структура каталогов `~/.fcpp-agent/{db,projects}`.

## [x] Этап 2: CMake & compile_commands
* [x] Детерминированный локатор `compile_commands.json` (3 источника: flag/config/`<root>/<default_build>`; FAIL при отсутствии).
* [x] Парсер `compile_commands.json` (nlohmann/json).
* [x] Инвентаризация TU в `index.db` (`file`, `compile_command`); реальный `index status`.

## [x] Этап 3: Project Manager и безопасность
* [x] Реестр проектов (`registry_project`), идентификация по каноническому пути, `db_path` из SHA-256.
* [x] `CProjectManager`: `open/close/current/list`; идемпотентный `open`; `find_symbols`; сессия.
* [x] `CExecutor` (ADR-08): whitelist, защита от инъекций, `fork`/`execvp`; **подключён к CLI** (`build`/`test`/`patch`).

## [x] Этап 4: libclang-индексация
* [x] `IClang` + `CClang22` (system-header фильтр; санитайз `-o/-c`); провайдеры в CMake (libclang 22).
* [x] `ICompilerIndex` + `CSqlCompilerIndex` (`base_types/ancestors/derived_types/overrides/base_method/dispatch_targets/file_includes/included_by`).
* [x] Прогрев: `symbol`/`symbol_location`/`include`/`inheritance`/`override`/`reference`/`call` (дедуп по USR/ребру).
* [x] `index rebuild` в новую generation; точный `cursor_at` (getLocation + getCursor).

## [x] Этап 4.1: Stale index & pointwise refresh
* [x] Детекция: mtime+size → SHA-256; транзитивные include → dirty TU; add/drop; инвалидация окружения.
* [x] Точечный `refresh` (только dirty TU), `relink_parent_ids`, обновление хэшей; `open` авто-refresh.

## [x] Этап 5: Tools и query layer
* [x] `ITool` + `CIndexTool` + `CToolRegistry`; инструменты: `outline/callers/callees/refs/includes/overrides/dispatch/def/ancestors/derived/search/source`.
* [x] Типизированные ошибки `EErrorKind`; `CToolContext` (единое соединение).
* [x] CLI детерминированных запросов; DTO с default member initializers.

## [x] Этап 8a: Инфраструктурный минимум
* [x] `allow_read/write/exec/subtasks` (§47/§48) в конфиге, безопасные дефолты.
* [x] `fcpp doctor` (preflight через `CExecutor`); structured logging `ILogger`/`CLogger`.
* [x] Персистентный «текущий проект» (session: save/restore/clear).

## [x] Этап 6a: Детерминированное ядро запроса (без LLM)
* [x] `SIntent` IR (Area×Subject×Relation×Projection×Effect) + enum-массивы/парсеры.
* [x] `CRequestPipeline` — **весь конвейер в одном классе**: стадии + ворота + монотонная цепочка исходов.
* [x] `SRequestOutcome` (`OK/DEGRADE/CLARIFY/NOT_UNDERSTOOD/REFUSED/FAILED`); дефолт неизвестного → `NOT_UNDERSTOOD`.
* [x] Evidence Planner — table-driven `Intent → [tools]` (14 маршрутов); CLI `route` (проверка без LLM).
* [x] Benchmark уровня A (детерминированный): `pipeline_test` (5 кейсов).
* [ ] Context Budget (§64) — лимиты и top-N в упаковке Evidence.

## [x] Этап 7a: Patch-pipeline без LLM
* [x] `SPatch` + `CPatchParser` (unified diff); `CPatchValidator` (dry-run, escape-guard); `CPatchApplier` (атомарно + откат).
* [x] Policy: `allow_write` + Y/N (`--yes`); CLI `patch`, `build`, `test`.

## [x] Этап 8b: Agent API (тонкий CLI, §93)
* [x] `IAgentApi` + `SApiRequest/SApiResponse` + `EApiMethod`; `CAgentApi` (project/index/build/tests/tool/patch/doctor/config.reload).
* [x] CLI — тонкий клиент (argv → SApiRequest → CAgentApi); API не экспонирует `Intent/Area/Relation` (§89.39).

## [x] Этап 6b: LLM-биндинг
* [x] `IModelApi` + DTO; `CUrlTransport` (libcurl); `CEndpointUtil` (base_url до `/v1` + `/chat/completions`).
* [x] `CModelOpenAICompat` (SSE), `CModelClaude` (`/v1/messages`); `CModelFactory` (main/light/remote, `same_as_main`).
* [x] `CIntentAnalyzer` (XML→Intent) + `CAskService` (lite→pipeline→main) + CLI `ask --stream`.

## [x] Конфигурация (расширение)
* [x] Все ключи: модели (`model_main/light/remote_*`), `db_dir`, `default_build`, `tool_*`, `enable_tidy`, `allow_*`.
* [x] `init`: нет файла → полный; есть → merge недостающих; `reload_config()` + CLI `reload`.

## [x] Контракт `ask` и разрешение курсора (ADR-13, ADR-14, §94)
* [x] **ADR-14/§94**: клиент `ask` шлёт только `text + editor{file,cursor}`; внутренние сущности (`Area/Relation/Projection/Tool/target`) выводит ядро. `EApiMethod::Ask`; CLI вызывает через `CAgentApi` (§93); команда `route` удалена.
* [x] **ADR-13**: вид (`m_kind`) задаёт фраза и не затирается курсором; курсор разрешает имя как ближайшую охватывающую сущность запрошенного вида (§21); вызов `resolve_cursor` только при пустом имени; промпт lite запрещает выдумывать имя для дейктики.
* [x] `IClang`/`CClang22`: `EFocusKind` + `cursor_at(..., focus)` с подъёмом по семантическим родителям.
* [x] Устранена коллизия `EEffect` (Intent) ↔ исполнительский enum → переименован в `EExecEffect`.
* [x] Прогон: `ask "Что делает этот класс?"` @ `shape.hpp:25:15` → `UNDERSTAND CLASS dogfood::model::Shape / SELF / EXPLANATION`. CTest 43/43.

## Обкатка (tooling)
* [x] Проект вне репозитория: `/home/artem/projects/outer/sandbox/dogfood`.
* [ ] TODO: fixture-набор `tests/fixtures/` (минимальные проекты + зафиксированный `compile_commands.json`).

---

## Conversation domain model and plugin protocol
* [x] Add public DTOs for structured questions and options; expose them in JSON protocol.
* [x] Add `question.shrink` API contract with a no-op implementation (`false`, no mutation).
* [ ] Complete question/answer lifecycle and wire structured clarification production to all ambiguity paths.
* [ ] Implement `shrink_question` truncation after the chat vector becomes the authoritative dialogue representation.

## Осталось (не блокирует текущий функционал)
* [ ] **7b** — полный цикл изменения: LLM генерирует патч → конвейер 7a (+ каскад ADR-10, hard-stop).
* [x] **6c (каскад)** — детерминированный `CModelCascade` (триггеры `EGlitchKind`, бюджет 1 ретрай/уровень, передача ошибки наверх, hard-stop); `CEscalatingIntentAnalyzer` (lite→main→remote) подключён в `ask`. Тест `cascade_test`; CTest 43/43.
* [x] **6c (answer-каскад)** — генерация ответа по Evidence использует общий `CModelCascade` (lite→main→remote), ограниченный бюджет попыток и повтор с диагностическим контекстом.
* [ ] **6c (benchmark B)** — LLM-классификация (§71–72).
* [x] **llama.cpp**: выполняется через `llama-server` (OpenAI-compatible) → используется существующим `CModelOpenAICompat`, отдельный HTTP-клиент не нужен. Имя модели для локального сервера опционально (`is_available()` = только base_url).
* [x] **Prompt cache**: обеспечивается `llama-server` (cache_n в timings) при стабильном префиксе; наш промт держит неизменяемый system-prefix.
* [x] **Первый полный цикл на реальной модели**: `ask` → lite-модель (`Qwen2.5-Coder-14B`) → Intent → pipeline → Evidence → main → ответ.
* [x] Answer-каскад: эскалация генерации ответа по Evidence подключена к `CModelCascade`.
* [ ] Benchmark B: собрать реальные промахи классификации в regression-кейсы.
* [x] **Evidence-подача** в main внутри `ask`: `CEvidenceCollector` (выполняет план → Zero-Window-секции), `format_evidence` → промт main. Тест `evidence_test`; CTest 39/39.
* [x] Post-patch автосборка внутри `patch` (флаг `--build`) + **COMPILATION FAIL → авто-откат** (детерминированный триггер ADR-10).
* [ ] **Context Budget (§64), первый этап интегрирован**: `CContextManager` подключён к каждому вызову ReAct, к Intent/subtask контексту и генерации ответа по Evidence; группы истории и tool rounds отбираются целиком, обязательный контекст при переполнении даёт hard-stop. Добавлены конфигурируемые per-role `model_{main,light,remote}_context_window_tokens` и `model_{main,light,remote}_output_reserve_tokens`; настройки передаются в упаковщик контекста, output reserve также задаёт `max_tokens` провайдера. Для ReAct и Intent в обычном ask добавлена ограниченная extractive compaction: последние 8 ходов остаются целыми, старые представлены цитатными выдержками до 3000 байт. `source` извлекает полуоткрытый диапазон libclang по строкам и колонкам, а не целые строки; координаты сохраняются в `SSourceSlice`. Полная история и SQLite event log не изменяются; семантическая LLM-суммаризация и точный tokenizer пока не реализованы.
* [ ] Подключение `CLogger` во все компоненты (сейчас интерфейс + файл).
* [x] `allow_subtasks` (§48) — двухфазная декомпозиция (ADR-16/ADR-17): Фаза A (lite → текстовые подвопросы) + Фаза B (`CIntentAnalyzer`) + единый dataflow-цикл в `ask` с накоплением Evidence. CTest 47/47.
* [x] **Качество декомпозиции B-002 (ADR-18)**: якорь через `ancestors`; классификация `UNDERSTAND`/`OVERRIDES` (+ пост-коррекция); маршруты Overrides/References → Source; фильтр по `Subject.kind`.
* [x] **Fan-out Subject→N (ADR-19, path A)**: `outline(класс) → dispatch_targets(метод) → source(реализация)` в `CEvidenceCollector`; лимиты 10/20. B-002 → **PASS**: вердикт «Все методы реализованы» + таблица Circle/Rectangle × area/name.
* [x] **B2 (ADR-20): курсор → символ через IClang в промпт lite**. Одиночный вопрос: `[cursor symbol: NAME (KIND)]` уходит в lite → субъект = `Rectangle::area` (B-003 PASS). Составной: подвопросы + якорь иерархии (B-002 сохраняет PASS).
* [x] `fcpp serve` (Agent API как процесс, JSON-RPC поверх stdio) — реализован (ADR-26).
* [x] **Conversation Context (§49/§50), replay подключён**: `ask(session_id)` восстанавливает user/assistant turns и `context_json` из `sessions.sqlite`; события остаются append-only, replay выделен в тестируемую функцию. Добавлены регрессионные тесты на восстановление между открытиями store, незавершённый turn и malformed event; protocol regression tests проходят CI, end-to-end integration tests добавлены: проверяется, что persisted context и предыдущие user/assistant turns после повторного открытия store попадают в следующий model prompt. State projection с provenance и атомарность обновления context/close реализованы; CI подтверждён зелёным на коммитах этих изменений. `save_session`/`restore_session` относятся только к проекту.
* [ ] **Роутинг planner(cloud)/worker(local) — ADR-24** (недоделка, ADR-27): нет расчёта blast radius из индекса, handoff-пакета, переключения по раундам, sticky-планировщика, тегирования раундов, бюджета cloud. Сейчас есть только эскалация по сбою (ADR-25).
* [ ] Порядок закрытия (ADR-27): сначала Conversation Context (§49) — базис; затем роутинг + blast radius + handoff (ADR-24/§7).
* [ ] Context Budget (§64): лимиты/`top-N` упаковки Evidence.
* [ ] Авто-выбор remote-модели вне эскалации — задел.

## Следующий этап: Conversation Context → Context Budget → Planner/Worker

Архитектурное решение зафиксировано в [ADR-28](decisions.md#adr-28-conversation-context-и-context-budget-manager).

Порядок реализации:
1. [x] Базовая инфраструктура и replay Conversation Context: `session_id`, lifecycle API, отдельное хранилище, восстановление turns и передача `context_json` в ask (§49/§50; ADR-27/28). State projection с provenance и транзакционность lifecycle/context событий реализованы; остаются protocol/integration tests.
2. [ ] `CContextManager`: selector подключён к ReAct, Intent/subtask и Evidence answer; per-role context-window limits/output reserve применяются. ReAct и Intent compaction старой истории покрыты unit-тестами; осталось добавить provider-specific точный подсчёт токенов и, при необходимости, модельную семантическую суммаризацию (§64; ADR-28).
3. [x] Подключить общий budget/context builder к ReAct и генерации ответа обычного `CAskService`, включая per-model budgets из конфига; [ ] добавить точный tokenizer.
4. [ ] После пунктов 1–3 — planner(cloud)/worker(local), handoff и blast-radius routing (ADR-24).

`1 GiB` не использовать как порог compaction: это не размер контекстного окна модели. История сохраняется полностью; сокращается только представление, передаваемое конкретному вызову модели.

## Известные ограничения
* `index.db`/`registry.db` — dev-схема без версионирования до релиза v1 (ADR-12); индекс — кэш (пересоздаётся).
* `ask` собирает Evidence по плану; при отсутствии данных — явный отказ «в индексе недостаточно информации» (§57).
* Среда: clang/libclang 19 (base) и **libclang 22** (`/usr/local/llvm22`, §2.2); libcurl 8.19; cmake 3.31.10; ninja 1.13.2.
