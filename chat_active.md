# Активный контекст проекта

## Актуальное состояние после проверки session_id (2026-10-28)

- HEAD ветки `dev` содержит пользовательские коммиты `daaf2fc` (`id_session fixed`) и `4cf377d` (`test failed. source fixed`), поверх прежних исправлений ask.
- `ask` требует непустой `session_id`; `project.open` возвращает `project_id` и активный `session_id`; `session.resume` требует открытого проекта; ошибка API сохраняет переданный `session_id` в ответе.
- Исправлена ошибка знака в проверке `bind_project()`: теперь отказ `bind_text()` не трактуется как успех.
- Последующие исправления: API больше не возвращает `Ok` при неуспешном ask без текста ответа; добавлен `NotUnderstood`; при разрешённом текущем курсоре история ответов ассистента исключается из preflight-классификации и intent chat-context; неразрешённый курсор в местоименном follow-up приводит к структурированному уточнению, а не к general-engineering fast-path.
- `CAskService::cursor_symbol()` в текущем исходнике использует libclang по пути, строке, колонке и compile arguments; `CSqlCompilerIndex::symbol_at()` не должен быть источником истины для курсорного контекста. Если live smoke-test всё ещё показывает старое поведение, сначала проверить, что запущен бинарник из актуального HEAD и индекс/compile database зафиксированы.
- `plugin_api.md` обновлён под контракт v2. CLI/IDE — клиент: `open` → сохранить `session_id` → каждый `ask` с тем же ID; при потере ID — `session.resume` после открытия проекта.
- CLI теперь печатает `session_id` после `open`; однократный `session resume` принимает путь проекта либо пытается восстановить ранее сохранённый проект. Протокольные ошибки также возвращают распознанный `session_id`.
- Для неподдержанного `VERIFY/LOCATIONS` добавлен честный fallback с рекомендацией выполнить `build` и `tests` (без заявления, что они уже запускались); CI `40577558871` зелёный.
- Для местоименного follow-up с разрешённым курсором маршрут принудительно переводится в codebase, а субъект берётся из текущего символа; при неразрешённом курсоре уточнение срабатывает до fast-path. CI `40578324291` и `40578455643` зелёные.
- `session.resume` больше не позволяет выбрать чужой проект по произвольному `projectId`: требуется открытый/восстановленный проект, а переданный ID сверяется с ним. CLI session ID/resume прошли CI `40577772069`, `40578515790`; regression-тест `40578545980` зелёный.
- `NotUnderstood` вместо ложного `Ok` добавлен в API error enum; исходный код прошёл CI `40576906840`. Протокольные ошибки сохраняют распознанный `session_id` (`40577429755`).
- Следующий шаг: проверить CI после последних изменений, затем тесты P0 (пустой NOT_UNDERSTOOD), P1 (курсор на `warm` при истории про `store_inventory`), P5 (неразрешённый курсор + «А что он делает?»), а также повторно прогнать 1–5 на неизменном `compile_commands.json`.


Обновлено: 2026-10-27
Репозиторий: https://github.com/Nechitaylenko-aa/isolatingServer
Рабочая ветка: `dev`
Текущий HEAD перед синхронизацией этого файла: `4568ed43625391e0cd09fc77c9fd51f640ead4c8`. CI run `40544183171` завершился успешно: сборка и 117/117 CTest прошли. Пакет уточняет обработку отсутствующего субъекта и сохраняет курсорный объект для последующих вопросов.

## Назначение

`fcpp-agent` — C++-агент для анализа и работы с существующими C++-проектами через индекс libclang/SQLite, LLM-модели, ReAct-инструменты и JSON-RPC-подобный протокол stdio. Публичный API и форматы запросов/ответов документируются в `plugin_api.md`.

## Принятые проектные принципы

- Классифицировать намерение по смыслу запроса моделью, а не словарями синонимов.
- Различать общий инженерный вопрос и вопрос, требующий знания конкретного репозитория. Отсутствие контекста редактора само по себе не делает вопрос общим.
- Не выдумывать сведения о репозитории. При недостатке данных — уточнять или честно обозначать ограничение.
- Ответы модели подвергать критической проверке; высокий риск или сбой проверки должен приводить к эскалации/отказу, а не к выдаче сомнительного текста как факта.
- Внешний протокол и `plugin_api.md` должны оставаться согласованными.
- Изменения делать пакетами, но исправления в ходе аудита проверять по одному пункту: изменение → тесты → разбор результата.
- Критически проверять и собственные архитектурные решения; отличать подтверждённый дефект от гипотезы.

## Недавние изменения маршрутизации и оценки ответов

Последние изменения в `dev`:
- `7d9a86c` — разделение general engineering и codebase intent.
- `9c8bace` — ReAct prompt для концептуальных вопросов.
- `d9f9a14` — повторное использование классификации и передача исходного вопроса.
- `1eaab4e` — маршрутизация до ReAct и risk gate для ответа.
- `f996d1e`, `94c36b6`, `a494017` — сохранение истории в общем ответе; в `94c36b6` был пропущен один аргумент вызова, исправлен в `a494017`.
- `4fa5337` — ReAct сообщает фактический уровень модели; quality gate отклоняет неизвестный уровень.
- `2a55d2d`, `e61b5e2`, `88e8b70`, `658400c`, `8285e69` — диагностика CTest и обновление интеграционных фикстур под буферизованный review.
- `f3845fb`, `d9eef8c`, `4e0d122` — интеграционный тест повторного высокого риска: Lite дважды отклоняется, генерация эскалирует на Main, независимый Remote-рецензент принимает ответ.
- `f79916d`, `e205b07`, `b573f51` — общий `CModelLevels`: единая проверка доступности и эффективной идентичности endpoint для answer, ReAct и intent cascade; тесты для `same_as_main` и provider type.
- `02579b8`, `9e333ec`, `2cd80ed`, `725cdbb`, `7c04b78`, `39214e9` — генерация/ревью ответов вынесены в `CAnswerService`, логирование контекста централизовано, `try_fast_path` и `answer_codebase_request` отделены от внешнего `ask`-маршрутизатора.
- `1f66fe1` — parser ревьюера принимает только один строгий JSON-объект, проверяет точную схему и непустую причину; добавлены тесты на лишний текст/объекты и границы порога.
- `999d73d` — протокол проверяет типы известных параметров и обязательные поля; `plugin_api.md` описывает валидацию.
- `a9cd4d2`, `54fe289` — реализован `question.shrink` как логическое усечение активной истории через append-only marker; исходный аудит-журнал сохраняется, проекции ходов/состояния и проверка вариантов скрывают хвост.
- `91754e6`, `f99ccfb`, `d219d70` — quality gate больше не принимает ответ без независимого ревьюера более высокого уровня; интеграционные фикстуры явно включают Remote для проверки успешного review.
- `9033ae1` — удалена недостижимая повторная проверка `clang == nullptr` в `CAskService::cursor_symbol`.
- `86dd7a7` — неизвестный непустой `subject kind` теперь отклоняется вместо молчаливого выбора `Symbol`.
- `eb052f8` — проверка пути патча использует `weakly_canonical` и относительные компоненты; добавлены тесты против sibling-prefix и symlink escape.
- `6803c68`, `aa7c546` — удаление файла теперь проверяет hunks по оригинальному содержимому и отклоняет отсутствующие/несовпадающие hunks для непустого файла.
- `ca1abcb`, `5476eca`, `27a38a1` — unified diff parser валидирует фактические old/new line counts, строго разбирает числа диапазонов и отклоняет переполнение; исправлена старая тестовая фикстура с неверным заголовком hunk.
- `2757717`, `12c82eb` — `CPatchApplier` теперь сообщает о неудачном удалении при revert и о любом неполном rollback, включая неудачное восстановление ранее изменённых файлов.
- `a72dca6`, `35599ba` — intent XML parser проверяет envelope, уникальность обязательных полей, ровно один root и форму self-closing `subject`; malformed output должен идти в retry/escalation.
- `f364286` — `compile_commands.json` command tokenizer учитывает escaped spaces/quotes, whitespace/newlines и отклоняет незакрытые кавычки/висящий escape; CI `38136207888` зелёный.
- `38b43ee`, `daca688` — parser отклоняет пустую/пробельную строку `command`; regression-тесты прошли в CI `38137143640` и `38137149583`.
- R18: parser отклоняет пустую/относительную `directory` и пустой `file`; regression-тесты прошли CI `38137372514`.
- D1: базовый JSONL-журнал `~/.fcpp-agent/logs/agent.jsonl` с `request.start`/`request.finish` (коммит `57ceac2`).
- D1.2: добавляется коррелированная трассировка внутренних этапов `ask`: состояние проекта/индекса, intent scope, контекст, ReAct, intent, pipeline, evidence и answer. CI текущего пакета ещё не проверен.

Оценка ответа:
- `CAnswerQuality` разбирает JSON с `verdict`, `risk_score` 0..100 и `reason`.
- Порог эскалации сейчас равен 35; невалидный ответ ревьюера трактуется как отказ проверки.
- `CAskService` буферизует ответ до прохождения quality gate, чтобы не показать пользователю первую версию до эскалации.
- Оценка — эвристика модели, а не калиброванная вероятность истинности. Пока тесты в основном проверяют парсер, а не поведение всего маршрута.

## Проверки CI на момент сохранения

- Run `38129408961` для `d9f9a143...` завершился успешно.
- Run `38130058000` для `94c36b6...` завершился ошибкой компиляции: вызов `answer_general_engineering` без нового аргумента `history`. Исправлено в `a494017`.
- Run `38131774012` для `8285e69` завершился успешно: Build и все 94 CTest прошли. В workflow добавлен per-test timeout 60 секунд и verbose output.
- Run `38132086844` для `4e0d122` завершился успешно: Build и все 95 CTest прошли, включая тест эскалации качества.
- Run `38132461130` для `b573f51` завершился успешно: Build и все 98 CTest прошли, включая новые тесты общих правил идентичности endpoint.
- Run `38133090461` для `39214e9` завершился успешно: Build и все 98 CTest прошли после разделения `CAskService` и `CAnswerService`.
- Run `38133271737` для `1f66fe1` завершился успешно: Build и все 100 CTest прошли после ужесточения parser контракта.
- Run `38133548589` для `999d73d` завершился успешно: Build и все 102 CTest прошли после строгой валидации протокола.
- Run `38133844643` для `54fe289` завершился успешно: Build и все 104 CTest прошли, включая shrink и проекции активной истории.
- Run `38134653137` для `d219d70` завершился успешно: Build и все 105 CTest прошли; тесты ответа используют независимого Remote-рецензента.
- Run `38134782284` для `9033ae1` завершился успешно: Build и все 105 CTest прошли после удаления недостижимой ветки.
- Run `38134954191` для `86dd7a7` завершился успешно: Build и все 106 CTest прошли, включая regression-тест неизвестного `subject kind`.
- Run `38135074379` для `eb052f8` завершился успешно: Build и все 107 CTest прошли, включая sibling-prefix и symlink escape.
- Run `38135253937` для `aa7c546` завершился успешно: Build и все 108 CTest прошли, включая проверку deletion hunks.
- Run `38135527753` для `27a38a1` завершился успешно: Build и все 109 CTest прошли после строгой проверки hunk counts.
- Run `38135690384` для `12c82eb` завершился успешно: Build и все 110 CTest прошли, включая проверку неудачного revert.
- Run `38135847678` для `a72dca6` завершился успешно: Build и все 111 CTest прошли, включая строгую проверку intent XML.
- Run `38135961263` для `35599ba` завершился успешно: Build и все 111 CTest прошли после теста на дублирующий root.
- Run `38136207888` для `f364286` завершился успешно: шаги Build и Test прошли; подробный счётчик тестов в доступной сводке не указан.
- Более ранние запуски зависали на старом тестовом контракте; после обновления mock-ответов текущий прогон зелёный.

## Текущий санитарный час

1. Контекст сохранён здесь.
2. Результаты аудита и последовательность исправлений ведутся в `ref_todo.md`.
3. Исправлять пункты строго по одному и после каждого проверять сборку/тесты.
4. При переходе в новый чат сначала прочитать этот файл и `ref_todo.md`, затем проверить актуальный HEAD и CI — этот файл является снимком, а не истиной о более позднем состоянии.
5. R17 завершён; R18 реализован в текущем пакете и ожидает CI. Локальный каталог репозитория в текущем runtime отсутствует, а прямой `git clone` недоступен из-за DNS; файлы и CI проверялись через GitHub connector именно в ветке `dev`.


## Следующий практический этап: диагностика LLM
- D1 в текущем пакете: базовая JSONL-телеметрия жизненного цикла API.
- После зелёного CI: подключить trace-контекст к CAskService, CReActAgent, intent analyzer, model clients, Evidence и quality review.
- Затем прогнать набор реальных LLM-сценариев и собирать regression-кейсы.


## Контекст последующих вопросов и защитные меры (2026-10-27)

- Важный сценарий: вопрос с курсором «Тут есть ошибки?» → следующий вопрос без курсора «А что он делает?». При наличии сессии история должна позволить восстановить обсуждаемый символ.
- `CIntentAnalyzer` теперь получает явные правила: местоимение разрешается по недавнему диалогу только при одном однозначном антецеденте; неоднозначность не должна разрешаться простым выбором самого последнего символа.
- `CRequestPipeline::resolve_subject` переводит пустой субъект из `NotUnderstood` в `Clarify`; `CAskService` формирует явный вопрос о том, какой класс/метод/функция имеется в виду.
- Добавлено событие `ask.cursor`: фиксирует наличие курсора, факт разрешения символа и иерархического якоря без записи имени символа или пути.
- Добавлены regression-тесты: пустой субъект требует уточнения; контекст чата сохраняет предыдущий вопрос и ответ для разрешения местоимения.
- Ограничение: unit-тест контекстного источника подтверждает сохранность истории, но фактическое разрешение местоимений остаётся поведением LLM; нужен ручной тест на целевой конфигурации модели. Параллельно проверять, что классификация follow-up не уходит в general-engineering route, минуя evidence pipeline.


### CI environment note — 2026-10-27

- Run `40542209000` for commit `332ad99` failed before CMake/build/tests: `ubuntu-latest` resolved to Ubuntu 26.04 (resolute), and `apt-get update` rejected the LLVM apt repository because its signing key `15CF4D18AF4F7421` was unavailable. This is an infrastructure/dependency-bootstrap failure, not a compiler/test result.
- Workflow runner is pinned to `ubuntu-24.04` to avoid an unreviewed major runner-image migration; the follow-up commit will trigger a fresh CI run.


## Сохранение курсора в истории диалога (2026-10-27)

- В событиях user уже сохранялись editor file/line/column, но реконструкция `SConversationTurn` отбрасывала эти поля. Теперь координаты переживают replay истории.
- Перед маршрутизацией `ask` агент пытается восстановить символ по курсору в четырёх последних ходах и добавляет только имя/вид сущности как `[Previous turn cursor symbol: ...]` в временную проекцию контекста. Исторические события не переписываются.
- Добавлено событие `ask.history_cursor` с количеством проверенных ходов и разрешённых символов; пути и имена не попадают в журнал.
- Тесты проверяют replay координат. Регрессионный тест streaming обновлён: после отказа ReAct от неподтверждённого ответа корректным результатом может быть `CLARIFY`, а не только ошибка; запрещено выдавать/стримить отвергнутый текст.


### Follow-up cursor-context CI correction

- CI for `b6a2d67` found `-Wmissing-field-initializers` after adding editor metadata to `SConversationTurn`. The type now has a constructor with defaults so existing three-field turn creation remains source-compatible and warning-clean.
- The historical cursor marker now uses an actual newline and the Intent prompt explicitly distinguishes a previous-turn cursor from the current editor cursor.


## Проверка пакета контекста (2026-10-27)

- Коммит `4568ed4` включает совместимость `SConversationTurn` с прежними трёхполевыми инициализаторами через конструктор с параметрами по умолчанию.
- CI run `40544183171`: Build успешно; 117/117 CTest прошли. Ранний запуск `332ad99` остановился на LLVM apt signing key, а первый прогон `515f9a9` дошёл до тестов и выявил устаревшее ожидание streaming regression; после фикса контракта уточнения тест зелёный.
- Осталось проверить на реальном LLM: (1) «Что делает этот метод?» без курсора и без antecedent → уточнение; (2) вопрос с курсором → follow-up без курсора разрешает тот же символ; (3) два возможных antecedent → уточнение; (4) общий вопрос RAII не захватывает курсорный контекст.


### D1.3 — Диагностика отказа открытия проекта
- **Проблема:** `project.open` возвращал только общий отказ, а JSONL не показывал этап сбоя.
- **Изменение:** `CProjectManager::open` сообщает стабильный `failure_stage`; `CAgentApi` пишет `project.open.failed` / `project.open.succeeded` с correlation ID. Ответ API также включает этап. В журнал не попадают пути, промпты или содержимое исходников.
- **Этапы:** канонизация корня, проверка каталога/CMake, поиск и разбор compile commands, миграция/открытие БД, Clang backend, инвентаризация, прогрев/обновление индекса, паспорт и регистрация проекта.
- **Тест:** добавлена API-регрессия для отсутствующего корня проекта.
- **Статус:** подготовлено для CI; следующий этап — длительности внутренних стадий ask/LLM/evidence и причина неразрешённого курсора.


## Исправление дедупликации compile inventory (2026-10-27)

- Подтверждённый сбой: `project.open` остановился на `store_compile_inventory`, потому что один TU встречается в нескольких targets, а схема запрещает дубли `(path, generation)`.
- `CSQLiteIndex::store_inventory` нормализует путь и сохраняет первую команду для каждого уникального TU; счётчик `index_generation.file_count` отражает реально сохраняемые файлы, а не исходный размер массива.
- Добавлен регрессионный тест с одним файлом и двумя compile commands, проверяющий одну строку `file`, одну `compile_command`, корректный file_count и детерминированный выбор первой команды.
- CI для предыдущего коммита `93c4b2c` зелёный. Этот пакет дедупликации ещё ожидает CI.
- Реальные LLM-логи также выявили отдельный дефект маршрута: ReAct не разрешает курсорный символ, многократно получает `SymbolNotFound`, затем quality gate отклоняет ответ и evidence pipeline не находит маршрут для классифицированного intent. Follow-up без курсора доходит до CLARIFY; отдельный вызов ревьюера завершается timeout. Это следующий пункт после стабилизации open.


## Пакет нормализации путей и выбора cursor symbol (2026-10-27)

- Введён единый `CProjectPaths`: разрешает абсолютные пути лексически и относительные только относительно явно заданной абсолютной базы; не использует скрыто `current_path()`.
- `CProjectManager::resolve_path` разрешает пути редактора от канонического корня активного проекта и возвращает пустой путь без активного проекта.
- Парсер `compile_commands.json` нормализует `directory` и разрешает `file` относительно него. `CSQLiteIndex::store_inventory` использует один и тот же абсолютный путь для дедупликации, stat и записи в SQLite; проверка выхода за корень компонентная, не строковым префиксом.
- `CAskService` использует разрешённый путь и для cursor lookup, и для аргументов компиляции, и для libclang fallback; иерархический якорь следует тому же правилу.
- `CClang22::cursor_at(..., Innermost)` теперь не останавливается на параметре: параметр сохраняется как fallback, а ближайший callable выше по AST становится контекстным символом.
- `CRequestPipeline::resolve_subject` удаляет дублирующиеся qualified names в вариантах уточнения, сохраняя порядок.
- Добавлены regression-тесты на относительный/абсолютный путь и на курсор внутри параметра метода.
- CI `40561659680` (нормализация путей и выбор enclosing method) и `40562241290` (разделение light/full open) зелёные. Smoke на целевом проекте ещё нужен; последующий аудит выявил, что cache-first в `CAskService` всё равно обходил libclang.


## Разделение light context и полного project.open (2026-10-27)

- `has_current()` означает, что идентификатор/контекст проекта известен; после `restore_session_light()` это может быть read-only контекст без полного открытия.
- Добавлен `is_opened()`: становится true только после успешного полного `open()`; сбрасывается при `close()`; light restore выставляет false.
- Разрешение editor path, поиск символов, изменение паспорта и операции refresh/rebuild требуют полного открытия.
- `handle_ask` и команды, которым нужен рабочий проект/индекс, выполняют полное восстановление сессии, если до этого был только light context. Если light context был известен, но полноценное открытие не удалось, ask завершается явной ошибкой вместо работы с неготовым проектом.
- Добавлен тест: `restore_session_light()` даёт `has_current=true, is_opened=false`; обычный `restore_session()` переводит контекст в полностью открытый.
- Статус: реализация и тест добавлены; ждёт CI.


## Cursor-cache audit follow-up (2026-10-27)

- Critical review found that `CAskService::cursor_symbol` still returned `CSqlCompilerIndex::symbol_at(file,line,column)` before invoking libclang. `symbol_at` explicitly ignores `column` and chooses a symbol by line span, so the cache could return the parameter or another same-line declaration and bypass the tested `CClang22::cursor_at` behavior. The cache-first result is removed; cursor context now uses libclang with normalized file path and indexed compile arguments.
- Clarification options are deduplicated at the API text-to-DTO boundary too, because model-authored clarification text does not necessarily pass through `CRequestPipeline::resolve_subject`. Pipeline regression now includes overloads with the same qualified name and requires exactly two unique choices.
- A known project context that has not passed full `project.open` now fails closed in `CRequestPipeline::resolve_subject`; a direct regression test covers light restore followed by symbol resolution.
- This package is committed to `dev`; verify its CI before claiming these latest changes are green. Next manual smoke: cursor on `commands` in a method parameter; confirm returned cursor hint names the enclosing method; follow-up question options contain no duplicates.


## Ask regressions A/B/C and path resolver cleanup (2026-10-27)

- A: deterministic preflight asks for a method name/cursor for common Russian deictic method questions when there is neither a current cursor nor a historical cursor referent. The existing API clarification adapter should return `CLARIFY` with a question.
- A/B: if the current request supplies a cursor but it cannot be resolved, old cursor-bearing turns are not injected as a replacement symbol. This prevents stale-symbol substitution.
- B: final evidence answer generation receives the raw current user message; richer context remains available for intent resolution and evidence collection, but is no longer misrepresented as the question to answer.
- C: public clarification options are deduplicated after whitespace/case normalization, covering duplicate display labels in cache-first paths.
- `CProjectPaths::resolve` moved out of the header into `src/core/CProjectPaths.cpp`; CMake includes the implementation.
- Regression tests added for unnamed-method clarification and path resolution. CI pending.

- Дополнение B: ReAct/general-engineering fast-path отклоняет ответ, текстуально совпадающий с одним из последних трёх ответов ассистента; пишет `ask.answer.duplicate_history`, очищает кандидат и продолжает через evidence pipeline.
