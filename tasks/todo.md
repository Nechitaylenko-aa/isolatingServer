# fcpp-agent: Бэклог

**Переписан целиком** — прежняя версия почти полностью описывала
`CAskService`/`CIntentAnalyzer`/`CModelCascade`/subtasks/собственную
мультисессионность Conversation Context. Эта система снесена целиком
(decisions.md №2: «два параллельных маршрутизатора в одной сборке. Зелёный
CI подтверждал поведение, недостижимое из `ask`») задолго до того, как в
этом файле отражалось её удаление — отсюда и нынешняя переработка: чтобы то
же самое не повторилось с этим документом. Ссылка на
`plans/react-loop-todo.md` в прежней шапке вела на несуществующий файл —
тоже убрана.

Порядок реализации (ADR-12 из decisions.md): детерминизм вперёд, LLM —
последним. Решения и их обоснование — только в `tasks/decisions.md`, здесь
не дублируются.

## Живая архитектура (проверено по коду, не по памяти)

Инфраструктурный слой — без изменений с прежней версии документа, актуален:
* `CProjectManager` (open/close/current/list, идентификация по каноническому
  пути, персистентная сессия проекта).
* `CSQLiteIndex` + `IClang`/`CClang22` — индексация, `detect_stale`/
  `refresh_if_stale` (mtime+size → хэш-подтверждение, точечный refresh dirty
  TU, не полный rebuild).
* `ITool`/`CToolRegistry` — детерминированные read-only инструменты над
  индексом (`outline/callers/callees/references/includes/overrides/
  dispatch/definition/ancestors/derived/search/source`).
* `SPatch`/`CPatchParser`/`CPatchValidator`/`CPatchApplier`/`CExecutor` —
  детерминированный конвейер патча (парсинг unified diff → валидация §46 →
  атомарное применение → опциональная сборка `EFFECT_WRITE_BUILD` → откат).
* `CConfigManager`/`CLogger`/`CConversationStore` — конфиг, структурное
  логирование, хранилище сессий (session = project, не отдельный lifecycle
  клиента — decisions.md №20).
* `CLlmLayer`/`SLlmRoleConfig` — слой моделей, роли `CoderSmart/CoderLocal/
  ThinkerSmart/ThinkerLocal`, эскалация и бюджет окна по роли.
* `IAgentApi`/`CAgentApi`/`CProtocolServer` — протокол (`project.*`,
  `index.*`, `build.build`, `tests.run`, `tool`, `doctor`, `ask`,
  `session.update_context`, `project.profile.set`, `question.shrink`) и
  `serve` (JSON-RPC поверх stdio, персистентный процесс).
* `CRequestOrchestrator` — единое дерево `ask`: применимость курсора/файла/
  истории (6 маршрутов) → `EOrchestrationIntent{Answer, Edit}` → ответ
  текстом (`generate_answer`) либо правка (`generate_edit`, решение №19).

## Открытое

* [ ] **Чистка документации** — `requirements.md` всё ещё перечисляет
  `build.configure`/`tidy.run`/`request.cancel` как часть протокола в
  нескольких списках; реализации этих методов я не нашёл. Нужно либо найти,
  либо убрать как ещё один слой того же дрейфа, что был с `session.*`.
* [ ] **Intent-классификатор: пересмотреть, чем он принимает решение.**
  Сейчас `classify_intent()` в `CRequestOrchestrator` использует
  `ELlmRole::ThinkerLocal` + общий `EPromptType::YesNo`. Рассматривается
  замена на выделенную модель именно под эту задачу (вариант: Joey 1,
  2B decision-making модель от Base Labs — требует проверки пригодности и
  интеграции отдельно от существующих ролей, не факт, что вписывается в
  текущую схему `CoderSmart/CoderLocal/ThinkerSmart/ThinkerLocal`).
* [ ] **Откат правки на основе системного git**, не только
  `CPatchApplier::revert`. Черновая идея: теневой git-репозиторий под
  `/tmp/fcpp/<project-id>/...`, снапшот перед применением патча, откат —
  обычный `git reset`/`git checkout` в теневом репо, без прикосновения к
  рабочему дереву проекта напрямую через patch-уровневый revert.
  `plans/git-snapshot-rollback.md` — более ранняя, многошаговая версия той
  же идеи (откат не одного патча, а нескольких шагов сессии); решить, эта
  задача — её первая очередь или отдельный, более узкий механизм.
* [ ] **Тесты на реальную сборку/откат** (`allow_exec=YES` путь в
  `generate_edit`). Во всём проекте нет ни одного теста, который взаправду
  запускает `CExecutor` — ни для `build.build`, ни для патча. Нужно решить,
  делать интеграционный тест с реальным тулчейном или вводить инъекцию
  `IExecutor*` в `generate_edit` для подмены в юнит-тестах.
* [ ] **`serve` не обкатан на живом трафике.** CI зелёный на уровне сборки
  и юнит-тестов; персистентный режим (на который рассчитана сессионная
  модель после decisions.md №20) ещё не использовался в реальной работе —
  план автора: сначала отладка через одноразовый CLI `ask`, `serve` — после.
* [ ] **Context Budget** — `plan_budget`/`fit_material_to_budget` в
  `CRequestOrchestrator` считают по роли, которая реально отвечает
  (decisions.md №18), но единого бюджетного модуля уровня прежнего
  `CContextManager` нет; не факт, что он нужен в этой форме — решить при
  следующем столкновении с реальным переполнением контекста, не заранее.

## Известные ограничения

* `index.db`/`registry.db` — dev-схема без версионирования до релиза v1.
* Среда: libclang (версия — см. `CMakeLists.txt`, `REQUIRED`), libcurl,
  cmake, ninja — точные минимальные версии не перепроверялись при этой
  переработке, доверять `CMakeLists.txt`, а не этому списку.
