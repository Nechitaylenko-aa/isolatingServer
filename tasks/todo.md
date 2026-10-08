# fcpp-agent: Глобальный Бэклог

## [x] Этап 0: Архитектурное проектирование и устранение неопределенностей
* [x] Сформировать предложения по Блоку 1 (Инфраструктура и CLI) в `decisions.md`
* [x] Сформировать предложения по Блоку 2 (Индексация и C++ Парсинг) в `decisions.md`
* [x] Сформировать предложения по Блоку 3 (LLM, Промптинг и Безопасность) в `decisions.md`
* [x] Утвердить базовые абстракции (интерфейсы `IIndex`, `ILLMClient`, `IExecutor`) до реализации плагинов.

## [x] Этап 1: Скелет проекта и CLI Инфраструктура
* [x] Инициализировать проект (C++20 + CMake + Ninja, `-Wall -Wextra -Werror`, FetchContent: nlohmann/json, sqlite3).
* [x] Настроить парсинг базовых CLI-команд (`init`, `open`, `close`, `projects`, `status`, `index`).
* [x] Настроить инициализацию SQLite (миграции + схема; `CSchemaMigrator`).
* [x] Конфигурация агента в стиле rc.conf (`CConfigManager`, bootstrap + load/validate).
* [x] Логирование — отсутствует (открытая задача).

## [x] Этап 2: CMake & Сборщик контекста
* [x] Реализовать модуль `compile_commands.json` (детерминированный локатор, 3 источника; FAIL при отсутствии).
* [x] Парсер `compile_commands.json` (nlohmann/json).
* [x] Инвентаризация translation units в `index.db` (`file`, `compile_command`), реальный `index status`.

## [x] Этап 3: Project Manager, безопасность и тесты
* [x] Реестр проектов (`registry_project`), идентификация по каноническому пути, `db_path` из SHA-256 (`CHash`).
* [x] `CProjectManager`: `open`/`close`/`current`/`list`; миграции per-project БД; идемпотентный `open`.
* [x] `CExecutor` (ADR-08): whitelist, защита от инъекций, `fork`/`execvp` (ещё не подключён к CLI).
* [x] Catch2 3.x: интеграционные тесты Project Manager и compile_commands (10 кейсов, CTest).

## [x] Этап 4: libclang-индексация (символы и связи)
* [x] `migrations/0004_relations.sql` — таблицы `include/inheritance/override/reference/call` (схема v4).
* [x] `IClang` + `CClang22` (libclang 22; hard-require, санитайз `-o/-c`, system-header фильтр).
* [x] `ICompilerIndex` + `CSqlCompilerIndex` (`base_types/ancestors/derived_types/overrides/base_method/dispatch_targets/file_includes/included_by`).
* [x] Прогрев кэша в `open()`: libclang → `symbol`/`symbol_location`/`include`/`inheritance`/`override`/`reference`/`call` (дедуп по USR/ребру).
* [x] Проверено на dogfood: 36 символов, Shape→Circle/Rectangle, `dispatch_targets(area)=3`.
* [x] `index rebuild` в новую generation; `cursor_at` (getLocation + getCursor, точный резолв).

## [x] Этап 4.1: Stale index & pointwise refresh
* [x] `migrations/0005_symbol_parent_usr.sql` (parent_usr для перелинковки без полного rebuild).
* [x] `SStaleReport` + `detect_stale`/`refresh_if_stale` + fingerprint `compile_commands.json`.
* [x] Детекция: mtime+size → подтверждение SHA-256; транзитивные include → dirty TU; add/drop; инвалидация окружения.
* [x] Точечный `refresh`: переиндексация только dirty TU и их заголовков, `relink_parent_ids`, обновление хэшей.
* [x] `open` авто-refresh при устаревании; `index status` показывает `stale`/`dirty`.
* [x] Тест `stale_test`: изменение заголовка → 2 dirty TU → точечный refresh → индекс снова актуален. CTest 18/18.

## [x] Этап 5: Tools и query layer
* [x] `ITool` + `CIndexTool` + `CToolRegistry` — типизированные инструменты (Task/Request/Result).
* [x] Инструменты: `outline`, `callers`, `callees`, `refs`, `includes`, `overrides`, `dispatch`, `def`, `ancestors`, `derived`, `search`, `source` (Zero-Window-срез).
* [x] Типизированные ошибки `EErrorKind` (Секция 3, enum-массив).
* [x] `CToolContext` — единое соединение (индекс владеет, граф делит).
* [x] CLI-команды детерминированных запросов + исправленная справка (`ask/build/test/tidy` помечены как нереализованные).
* [x] Устранён дефект неинициализированных полей DTO (default member initializers во всех публичных структурах).
* [x] Тесты: `tools_test` (реестр + dogfood), CTest 17/17.
* [ ] Deterministic Evidence Planner (Intent -> [tools]) — Этап 6.

## Порядок дальнейших этапов (ADR-12: детерминизм вперёд, LLM — последним)

## [x] Этап 8a: Инфраструктурный минимум
* [x] `allow_read`/`allow_write`/`allow_exec`/`allow_subtasks` в конфиге (§47/§48), безопасные дефолты.
* [x] `fcpp doctor` — preflight инструментов через `CExecutor` (ADR-06).
* [x] Structured logging (§60): `ILogger` + `CLogger` (файл вне проекта, enum-уровни).
* [x] Персистентный «текущий проект»: `open` сохраняет сессию, `status`/`index`/tools восстанавливают, `close` очищает.
* [x] Тест `stage8a_test` (permissions + session + doctor); CTest 21/21.
* [ ] Замечание: логирование реализовано, но пока не подключено во все компоненты (базово — интерфейс + файл).

## [ ] Этап 6a: Детерминированное ядро запроса (без LLM) — СЛЕДУЮЩИЙ
* [ ] `Intent` (Area/Subject/Relation/Projection/Effect), реестры маппинга (enum-массивы).
* [ ] Subject Resolver (надстройка над `cursor_at`), дедупликация неоднозначностей (§21/§50).
* [ ] Evidence Planner — table-driven `Intent -> [tools]`; проверка полноты tools на канонических вопросах.
* [ ] Context Budget (§64) — лимиты и top-N.

## [x] Этап 7a: Patch-pipeline без LLM
* [x] `SPatch` модель + `CPatchParser` (unified diff).
* [x] `CPatchValidator`: dry-run сопоставление контекста, защита от выхода за корень проекта, план изменений.
* [x] `CPatchApplier`: атомарное применение (all-or-nothing) с откатом при ошибке (§77).
* [x] Policy: `allow_write` (§47) + интерактивное Y/N (`--yes` для non-interactive).
* [x] CLI `patch <file|->`, `build` (cmake --build), `test` (ctest) через `CExecutor`.
* [x] Тесты `patch_test` (parse/apply/conflict/escape); CTest 24/24.
* [ ] Post-patch автосборка внутри `patch` (сейчас отдельная команда `build`).

## [x] Этап 8b: Agent API (тонкий CLI, §93)
* [x] `IAgentApi` + `SApiRequest/SApiResponse` + `EApiMethod` (типизированный API).
* [x] `CAgentApi` — методы: agent.status/capabilities, project.*, index.*, build, tests, tool, patch, doctor.
* [x] CLI переписан как **тонкий клиент** (argv → SApiRequest → CAgentApi); прямые вызовы core убраны.
* [x] API не экспонирует `Intent/Area/Relation/Tool` как обязательные (§89.39); `SApiRequest` с `m_payload` (патч).
* [x] Тест `agent_api_test` (open/status/tool/patch/close); CTest 25/25.

## [ ] Этап 6b: LLM-биндинг
* [ ] `ILLMClient`: llama-cpp (`prompt_cache`, стриминг), XML-обмен (Aider-style).
* [ ] `lite` заполняет `Intent`; `main` формирует ответ.

## [ ] Этап 7b: Полный цикл изменения
* [ ] LLM генерирует патч → конвейер 7a.

## [ ] Этап 6c: Каскад моделей и benchmark
* [ ] Cascading Lite → Main → Cloud (ADR-10).
* [ ] Benchmark классификации (§71–72).

## [x] Инфраструктура обкатки (tooling)
* [x] Обкаточный C++/CMake-проект вне репозитория: `/home/artem/projects/outer/sandbox/dogfood`
      (Shape → Circle/Rectangle: inheritance, overrides, call-graph, include-граф; Ninja; real `compile_commands.json`).
* [x] Проверено end-to-end: сборка dogfood → `fcpp-agent open <path>` → инвентарь 3 TU с реальными флагами
      (`-I.../src -std=gnu++20`) в `index.db`.
* [ ] TODO: fixture-набор в репозитории `tests/fixtures/` (минимальные проекты + зафиксированный `compile_commands.json`).
* Примечание по среде: доступны clang/libclang 19 (base) и **libclang 22** (`/usr/local/llvm22`, соответствует §2.2);
      cmake 3.31.10, ninja 1.13.2.
* Известный техдолг: `status`/`index status` в отдельном процессе не видят проект (нет персистентной сессии) → Этап 8.
