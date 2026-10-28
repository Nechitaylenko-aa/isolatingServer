# fcpp-agent v2.0

## Техническое задание и архитектурная спецификация

**Статус:** Draft / Implementation Specification
**Версия:** 2.0
**Назначение документа:** постановка задачи для CLI-агента LLM, выполняющего разработку `fcpp-agent`.

---

# 1. Назначение проекта

`fcpp-agent` — консольный AI-агент для помощи разработчику при работе с проектами на C++, использующими CMake.

Агент предназначен исключительно для задач, связанных с:

* пониманием существующего C++ кода;
* проектированием C++ решений;
* созданием нового C++ кода;
* изменением существующего C++ кода;
* удалением кода;
* анализом и диагностикой;
* сборкой;
* тестированием;
* статическим анализом;
* проверкой качества;
* работой с CMake/build-инфраструктурой проекта.

Агент не является универсальным shell-агентом.

Его инструменты, контекст, модель данных и логика маршрутизации должны быть ориентированы на C++/CMake-проекты.

---

# 2. Основные платформы и инструменты

## 2.1. Поддерживаемые ОС

Основная платформа:

* FreeBSD.

Дополнительная:

* Linux.

Архитектура приложения не должна содержать платформенно-зависимый код там, где это можно избежать.

Платформенные особенности должны быть изолированы в соответствующих abstraction layer.

---

## 2.2. Компилятор

Основная версия:

* clang 22.

В рамках v2.0 агент использует одну версию clang.

Переключение версии clang пользователем в рантайме не требуется.

Однако архитектура **обязана предусматривать возможность добавления поддержки другой версии clang без изменения существующего application-level кода**.

Нельзя строить архитектуру вокруг жёстко зашитого:

```text
clang22 -> конкретная реализация
```

Вместо этого должен существовать абстрактный интерфейс compiler/indexer backend.

Концептуально:

```cpp
class CompilerBackend;
class ClangBackend final : public CompilerBackend;
```

Конкретная версия clang должна быть деталью реализации backend.

---

## 2.3. Build system

Основные инструменты:

* CMake;
* Ninja.

Агент должен уметь работать с существующим CMake-проектом и использовать его существующую конфигурацию сборки.

По умолчанию build directory:

```text
build
```

Конкретное расположение build directory должно быть конфигурируемым.

---

# 3. Основные архитектурные принципы

Архитектура `fcpp-agent` должна соблюдать следующие принципы.

## 3.1. Агент не изменяет структуру проекта без необходимости

В исходном дереве проекта не должны создаваться:

* базы данных агента;
* индексы агента;
* логи агента;
* конфигурация агента;
* временные файлы агента;
* файлы контекста;
* служебные metadata.

Всё состояние агента хранится вне проекта.

Исключение:

если пользователь явно попросил создать/изменить файл проекта, это является рабочим результатом агента, а не служебным состоянием агента.

---

## 3.2. Проект является отдельной сущностью

Агент должен поддерживать несколько проектов.

Для каждого проекта существует собственный контекст.

Минимальная концептуальная модель:

```text
fcpp-agent
    |
    +-- Project A
    |      |
    |      +-- SQLite index
    |      +-- project metadata
    |      +-- build configuration
    |
    +-- Project B
           |
           +-- SQLite index
           +-- project metadata
           +-- build configuration
```

Переключение между проектами не должно приводить к повторному полному анализу проекта, если актуальный индекс уже существует.

---

## 3.3. SQLite — основной источник индексированного контекста

SQLite является основным persistent storage для семантического контекста проекта.

В SQLite должны храниться результаты анализа проекта, необходимые для выполнения типичных запросов агента.

При наличии требуемых данных агент должен использовать SQLite.

Если требуемых данных нет либо данные признаны недействительными/устаревшими, агент должен обратиться к индексатору на базе libclang.

Это принципиально важно:

**LLM не должна самостоятельно искать структуру C++ проекта по тексту файлов, если соответствующая информация может быть получена из индекса.**

---

## 3.4. Индекс прогревается при открытии проекта

При открытии проекта агент проверяет состояние индекса.

Если индекс отсутствует или требует обновления:

1. определяется необходимый объём индексирования;
2. запускается libclang;
3. результаты сохраняются в SQLite;
4. проект становится доступен для полноценной работы.

Индексирование может быть длительной операцией.

CLI должен показывать пользователю понятное состояние операции:

```text
Opening project...
Index: 73%
Parsing: src/model/model_profile.cpp
```

Конкретный механизм progress reporting может быть выбран при реализации.

---

# 4. Архитектурная схема

Общая архитектура:

```text
                         +----------------+
                         |      CLI       |
                         +-------+--------+
                                 |
                                 v
                       +-------------------+
                       | Conversation /    |
                       | Request Manager   |
                       +---------+---------+
                                 |
                                 v
                       +-------------------+
                       | Intent Analyzer   |
                       |       (LLM)       |
                       +---------+---------+
                                 |
                                 v
                       +-------------------+
                       | Intent Resolver   |
                       +---------+---------+
                                 |
                                 v
                       +-------------------+
                       | Subject Resolver  |
                       +---------+---------+
                                 |
                                 v
                       +-------------------+
                       | Plan Generator    |
                       |   deterministic   |
                       +---------+---------+
                                 |
                                 v
                       +-------------------+
                       | Tool Executor     |
                       +---------+---------+
                                 |
              +------------------+------------------+
              |                  |                  |
              v                  v                  v
          SQLite Index       libclang           Build System
              |                  |                  |
              +------------------+------------------+
                                 |
                                 v
                       +-------------------+
                       | Evidence          |
                       +---------+---------+
                                 |
                                 v
                       +-------------------+
                       | Response LLM      |
                       +---------+---------+
                                 |
                                 v
                       +-------------------+
                       | CLI response      |
                       +-------------------+
```

Для операций изменения:

```text
Intent
  |
  v
Plan
  |
  v
Evidence
  |
  v
LLM generates Patch
  |
  v
Patch validation
  |
  v
User approval / policy
  |
  v
Apply Patch
  |
  v
Build / Tests
  |
  v
Result
```

---

# 5. Разделение ответственности LLM и программы

Это один из главных архитектурных принципов проекта.

LLM не должна быть главным оркестратором всех инструментов.

В частности, не следует позволять небольшой локальной модели произвольно выбирать из большого количества tools:

```text
callers
callees
refs
source
build
tests
...
```

На этапе v2.0 инструментальный маршрут определяется программным кодом.

LLM выполняет три основные функции:

1. понимание пользовательского запроса;
2. генерация ответа по собранным доказательствам;
3. генерация изменения исходного кода.

---

# 6. Три роли моделей

Агент поддерживает три логические роли.

## 6.1. `lite`

Назначение:

* анализ пользовательского запроса;
* классификация;
* извлечение сущностей;
* определение намерения;
* обобщение;
* простое преобразование текста.

`lite` не должна использоваться как основной coding model.

---

## 6.2. `main`

Назначение:

* обычное программирование;
* генерация кода;
* генерация патчей;
* объяснение кода;
* решение задач средней сложности;
* обычный диалог с разработчиком.

---

## 6.3. `remote`

Удалённая облачная модель.

Предназначение:

* сложные задачи;
* архитектурные решения;
* сложный анализ;
* сложный refactoring;
* ситуации, когда локальная модель недостаточна.

В v2.0 remote API должен быть предусмотрен архитектурно, даже если часть его возможностей ещё не используется автоматически.

---

# 7. Model routing

Роли моделей не должны быть жёстко связаны с конкретной моделью.

Например:

```text
lite  -> model A
main  -> model A
remote -> model B
```

должно быть допустимо.

Также:

```text
lite -> model A
main -> model B
```

и:

```text
lite -> same_as_main
```

должно быть допустимо.

Таким образом, роли:

```text
lite
main
remote
```

и конкретные backend/provider должны быть разделены.

---

# 8. API моделей

Необходимо определить общий интерфейс LLM backend.

Концептуально:

```cpp
class LlmProvider {
public:
    virtual ~LlmProvider() = default;

    virtual LlmResponse complete(
        const LlmRequest& request) = 0;
};
```

Необходимо предусмотреть:

* system prompt;
* user prompt;
* structured response;
* streaming, если backend его поддерживает;
* model name;
* temperature/другие параметры, если они нужны;
* ошибки API;
* timeout;
* authentication;
* metadata.

Конкретный интерфейс должен быть определён так, чтобы application code не зависел от конкретного API.

---

# 9. Провайдеры моделей

Обязательные backend abstraction:

```text
LlamaCpp
OpenAICompat
Claude
```

При этом:

* llama-cpp обязателен для локальных `lite`/`main`;
* OpenAI-compatible API должен быть отдельным backend;
* Claude должен быть отдельным backend;
* добавление нового API не должно требовать изменения core agent logic.

Концептуально:

```text
LlmProvider
    |
    +-- LlamaCppProvider
    +-- OpenAICompatProvider
    +-- ClaudeProvider
    +-- ...
```

---

# 10. Конфигурация агента

Конфигурация хранится в:

```text
~/.fcpp-agent
```

Каталог содержит:

```text
~/.fcpp-agent/
    agent.conf
    projects/
    db/
    ...
```

Конкретная внутренняя структура служебных файлов может быть изменена реализацией, если сохраняются внешние требования.

---

# 11. Формат конфигурации

Конфигурационный файл:

```text
~/.fcpp-agent/agent.conf
```

не должен использовать:

* JSON;
* XML;
* YAML;
* INI.

Формат должен быть в стиле FreeBSD `rc.conf`.

Пример:

```text
# fcpp-agent configuration

model_main="http://127.0.0.1:8080"
model_main_name=""

model_light="same_as_main"
model_light_name=""

model_remote_url="https://routerai.ru/api/v1"
model_remote_type="openai-compat"
model_remote_name="..."
model_remote_key="..."

db_dir="/home/user/.fcpp-agent/db"
default_build="build"

allow_read="YES"
allow_write="YES"
allow_exec="YES"
allow_subtasks="NO"
```

---

# 12. Configuration API

Парсер конфигурации должен быть отдельным компонентом:

```cpp
class AgentConfig;
class ConfigLoader;
```

Core-код не должен парсить `agent.conf` непосредственно.

Необходимо поддержать:

* комментарии `#`;
* `key="value"`;
* boolean-like значения `YES/NO`;
* ссылки вида `same_as_main`;
* проверку обязательных параметров;
* диагностику ошибок конфигурации.

При ошибке:

```text
fcpp-agent: configuration error
fcpp-agent: invalid value for model_light
```

а не падение с исключением без объяснения.

---

# 13. Project Manager

Необходимо реализовать управление проектами.

Концептуальный интерфейс:

```cpp
class ProjectManager {
public:
    ProjectId open(const std::filesystem::path& path);
    void close(ProjectId id);
    void switch_to(ProjectId id);

    ProjectContext& current();
};
```

Проект должен идентифицироваться по каноническому пути либо другой стабильной идентификации.

Для каждого проекта должны быть доступны:

* путь;
* состояние индекса;
* SQLite database;
* build directory;
* compile configuration;
* время последней индексации;
* версия схемы индекса;
* версия indexer backend.

---

# 14. Project Context

`ProjectContext` объединяет все компоненты, относящиеся к одному проекту.

Концептуально:

```cpp
class ProjectContext {
public:
    ProjectId id() const;
    const ProjectConfig& config() const;

    Index& index();
    BuildSystem& build();
    ToolRegistry& tools();

    void open();
    void close();
    void refresh();
};
```

Нельзя передавать по всему приложению десятки отдельных параметров:

```text
project_path
db_path
build_path
clang_path
...
```

Вместо этого используется `ProjectContext`.

---

# 15. Индекс C++

Индекс должен строиться на базе libclang.

Он должен предоставлять информацию как минимум о:

* файлах;
* include;
* namespace;
* классах;
* struct;
* enum;
* typedef/using;
* функциях;
* методах;
* конструкторах;
* деструкторах;
* переменных;
* полях;
* параметрах;
* inheritance;
* overrides;
* references;
* call relationships;
* source locations.

Конкретный набор данных может расширяться.

---

# 16. SQLite

SQLite является persistent storage индекса.

Схема БД должна быть версионируемой.

Например:

```text
schema_version
project
file
symbol
symbol_location
inheritance
reference
call
include
compile_command
...
```

Это не является обязательной финальной схемой таблиц.

Главное требование:

**схема должна позволять эффективно реализовать инструменты агента без необходимости каждый раз запускать libclang.**

---

# 17. Актуальность индекса

Агент должен уметь определить, что индекс потенциально устарел.

Минимальная информация:

```text
file path
mtime
size
```

При необходимости могут использоваться hash/другие признаки.

Для изменившихся файлов индекс должен обновляться.

Полная переиндексация всего проекта не должна выполняться без необходимости.

---

# 18. Основная модель пользовательского запроса

Ключевая архитектурная идея v2.0:

**не создавать огромный enum из всех возможных пользовательских задач.**

Вместо этого запрос представляется несколькими независимыми измерениями.

Основные оси:

```text
Area
Subject
Relation
Projection
Effect
```

---

# 19. Area

На уровне v2.0 предлагается следующая таксономия:

```cpp
enum class Area {
    UNDERSTAND,
    DIAGNOSE,
    DESIGN,
    CHANGE,
    VERIFY,
    REVIEW,
    ENV
};
```

## `UNDERSTAND`

Понимание существующего кода.

Примеры:

```text
Что делает этот класс?
Кто вызывает эту функцию?
Где используется этот символ?
Объясни этот код.
```

Эффект по умолчанию:

```text
READ_ONLY
```

---

## `DIAGNOSE`

Поиск причины проблемы.

Примеры:

```text
Почему это падает?
Где ошибка?
Почему объект уничтожается здесь?
Почему не компилируется?
```

По умолчанию:

```text
READ_ONLY
```

Если пользователь явно требует изменения — intent должен перейти в `CHANGE`.

---

## `DESIGN`

Проектирование решения.

Примеры:

```text
Как лучше спроектировать этот интерфейс?
Как организовать этот компонент?
Нужен ли здесь отдельный класс?
```

По умолчанию:

```text
READ_ONLY
```

Результатом является предложение, а не изменение исходников.

---

## `CHANGE`

Изменение проекта.

Включает:

* modify;
* create;
* remove.

Необходимо предусмотреть subtype:

```cpp
enum class ChangeKind {
    MODIFY,
    CREATE,
    REMOVE
};
```

Например:

```text
переименуй
    -> CHANGE / MODIFY

добавь класс
    -> CHANGE / CREATE

удали мёртвый код
    -> CHANGE / REMOVE
```

---

## `VERIFY`

Выполнение проверок.

Например:

* build;
* tests;
* tidy;
* sanitizer;
* другие deterministic checks.

---

## `REVIEW`

Оценка уже собранной информации.

Например:

* code smells;
* thread safety;
* архитектурный аудит;
* потенциальные ошибки;
* качество реализации.

Отличие от `VERIFY`:

`VERIFY` получает доказательства через выполнение проверок.

`REVIEW` получает доказательства и оценивает их.

---

## `ENV`

Работа с окружением проекта.

Например:

* CMake;
* build configuration;
* dependencies;
* compile commands;
* CI;
* environment configuration.

Конкретный список v2.0 может расширяться.

---

# 20. Subject

`Subject` описывает объект, к которому относится запрос.

Например:

```cpp
enum class SubjectKind {
    PROJECT,
    FILE,
    MODULE,
    NAMESPACE,
    CLASS,
    STRUCT,
    ENUM,
    FUNCTION,
    METHOD,
    VARIABLE,
    FIELD,
    SYMBOL,
    BUILD_TARGET
};
```

Subject также содержит способ выбора объекта.

Например:

```cpp
enum class SubjectSelection {
    EXPLICIT_NAME,
    CURSOR,
    CURRENT_FILE,
    CURRENT_PROJECT,
    CONTEXT,
    INFERRED
};
```

---

# 21. Subject Resolution

Разрешение Subject должно быть детерминированным.

LLM может предложить:

```text
ModelProfile
```

но она не должна самостоятельно решать:

```text
/home/user/project/src/foo/bar/model_profile.hpp
```

если это можно установить из индекса.

Пример:

```text
Пользователь:
"объясни этот класс"

cursor:
model_profile.cpp:42

Resolver:
nearest enclosing class -> ModelProfile
```

Таким образом:

```text
"этот класс"
```

означает ближайший охватывающий класс.

---

# 22. Неоднозначность

Если найдено несколько подходящих объектов, система должна:

1. попытаться разрешить конфликт детерминированно;
2. если возможно — сделать явное допущение;
3. если риск ошибки велик — задать один уточняющий вопрос.

Необходимо избегать цепочки из нескольких уточняющих вопросов.

Предпочтительно:

```text
Понял так: ModelProfile::load()
Если имелся в виду другой load(), скажи.
```

либо:

```text
Я нашёл два класса ModelProfile:
1. ...
2. ...
Какой использовать?
```

---

# 23. Relation

Relation описывает отношение Subject к другим сущностям.

Минимальный набор:

```cpp
enum class Relation {
    SELF,
    CALLERS,
    CALLEES,
    OVERRIDES,
    REFERENCES,
    INCLUDES
};
```

При необходимости список может расширяться.

Примеры:

```text
кто вызывает функцию
    -> CALLERS

что вызывает функция
    -> CALLEES

где используется класс
    -> REFERENCES

что переопределяет метод
    -> OVERRIDES

что include-ит этот файл
    -> INCLUDES
```

---

# 24. Projection

Projection определяет, что именно пользователь хочет получить из найденных данных.

```cpp
enum class Projection {
    SIGNATURE,
    SOURCE,
    EXPLANATION,
    LOCATIONS,
    GRAPH
};
```

Примеры:

```text
сигнатура
    -> SIGNATURE

покажи исходник
    -> SOURCE

объясни
    -> EXPLANATION

где используется
    -> LOCATIONS

покажи зависимости
    -> GRAPH
```

---

# 25. Почему нет огромного Detail enum

Нельзя создавать:

```text
EXPLAIN_CLASS_CALLERS
EXPLAIN_CLASS_CALLEES
SHOW_METHOD_CALLERS
SHOW_METHOD_CALLEES
...
```

Комбинация строится автоматически:

```text
Area
+
Subject
+
Relation
+
Projection
+
Effect
```

Это позволяет сохранять таксономию маленькой и стабильной.

---

# 26. Effect

Effect описывает реальное воздействие операции.

Минимум:

```cpp
enum class Effect {
    READ_ONLY,
    WRITE_SOURCE,
    EXECUTE,
    WRITE_BUILD
};
```

Effect не должен свободно назначаться LLM.

Он выводится из `Area`, типа операции и policy.

---

# 27. Safety rule

При неоднозначности эффект должен снижаться.

Приоритет:

```text
READ_ONLY
    <
WRITE_SOURCE
    <
EXECUTE
    <
WRITE_BUILD
```

Точная ordering может быть уточнена, но общий принцип обязателен:

**сомнение не должно превращать безопасную операцию в опасную.**

Например, если запрос можно интерпретировать как:

```text
"проверь, что..."
```

он не должен автоматически становиться:

```text
"исправь..."
```

---

# 28. Intent IR

После анализа пользовательского запроса создаётся структурированное промежуточное представление.

Концептуально:

```cpp
struct Intent {
    Area area;

    Subject subject;

    Relation relation;
    Projection projection;

    Effect effect;

    std::optional<ChangeKind> change_kind;

    Confidence confidence;
};
```

`Intent` является границей между LLM и deterministic core.

---

# 29. Structured LLM output

LLM должна возвращать структурированный результат.

Например концептуально:

```text
area = UNDERSTAND
subject.kind = CLASS
subject.name = ModelProfile
relation = SELF
projection = EXPLANATION
```

Core обязан:

1. проверить допустимость значений;
2. разрешить Subject;
3. проверить совместимость комбинации;
4. вычислить Effect;
5. построить Plan.

LLM не должна иметь возможности напрямую вызвать произвольный системный tool.

---

# 30. Evidence Plan

После получения Intent создаётся детерминированный план получения доказательств.

Например:

```text
Intent:
    UNDERSTAND
    CLASS
    SELF
    EXPLANATION

Plan:
    outline(ModelProfile)
    get_source(ModelProfile)
    callees(public methods)
```

Другой пример:

```text
Intent:
    UNDERSTAND
    METHOD
    CALLERS
    LOCATIONS

Plan:
    resolve(method)
    callers(method)
```

---

# 31. Plan должен быть детерминированным

Для каждой допустимой комбинации:

```text
Area × Relation × Projection
```

должен существовать определённый маршрут получения данных.

Он может содержать условную логику:

```text
если данных в SQLite нет:
    запросить libclang
    обновить SQLite
    повторить query
```

Но LLM не должна менять этот маршрут.

---

# 32. Tool Registry

Все инструменты регистрируются типизированным образом.

Минимальный набор:

```text
get_source
outline
callers
callees
overrides
refs
includes
compile_obj
run_tests
tidy
apply_patch
```

---

# 33. Tool interface

Концептуально:

```cpp
class Tool {
public:
    virtual ~Tool() = default;

    virtual ToolName name() const = 0;

    virtual ToolResult execute(
        const ToolRequest& request,
        ToolContext& context) = 0;
};
```

Каждый tool должен иметь:

* имя;
* описание;
* тип входных параметров;
* тип результата;
* требуемый Effect;
* требования к ProjectContext;
* ошибки;
* возможность тестирования.

---

# 34. `get_source`

Назначение:

получить исходный текст сущности.

Input:

```text
SymbolId / SourceRange
```

Output:

```text
SourceEvidence
```

Не должен использовать произвольное чтение файла, если необходимый диапазон известен из индекса.

---

# 35. `outline`

Назначение:

получить структуру сущности.

Например для класса:

```text
class ModelProfile

public:
    load()
    save()

private:
    validate()
    ...
```

---

# 36. `callers`

Возвращает функции/методы, вызывающие указанную функцию.

Источник:

* SQLite;
* при отсутствии данных — libclang/index refresh.

---

# 37. `callees`

Возвращает вызываемые сущности.

---

# 38. `overrides`

Возвращает отношения inheritance/override.

---

# 39. `refs`

Возвращает места использования символа.

---

# 40. `includes`

Возвращает include relationships.

---

# 41. `compile_obj`

Получает/проверяет compilation information для translation unit.

Должен использовать compile configuration проекта.

---

# 42. `run_tests`

Запускает тесты проекта.

Агент не должен самостоятельно изобретать способ запуска тестов, если CMake предоставляет test configuration.

Основной путь:

```text
cmake / ctest / ninja
```

Конкретная реализация определяется проектом.

---

# 43. `tidy`

Инструмент запуска проверки clang-tidy либо соответствующего настроенного механизма.

Наличие clang-tidy в конкретной установке должно проверяться.

---

# 44. `apply_patch`

Единственный специальный инструмент изменения исходников в рамках LLM workflow.

LLM должна генерировать patch, а не произвольную последовательность shell-команд для изменения проекта.

Pipeline:

```text
generate patch
    ↓
parse patch
    ↓
validate
    ↓
check affected files
    ↓
policy
    ↓
apply
```

---

# 45. Запрещённая модель изменения

Нежелательно:

```text
LLM:
  run shell
  sed ...
  awk ...
  perl ...
  python modify.py
```

для обычного изменения C++ исходников.

Изменение исходников должно проходить через контролируемый patch mechanism.

---

# 46. Patch validation

Перед применением patch необходимо проверить:

* существование файлов;
* соответствие исходному содержимому;
* отсутствие неожиданных изменений;
* корректность диапазонов;
* количество изменённых файлов;
* Effect policy.

При конфликте patch не должен применяться частично.

---

# 47. Read / Write / Execute permissions

Конфигурация содержит:

```text
allow_read
allow_write
allow_exec
```

Минимальная семантика:

### `allow_read`

Разрешает агенту читать проект и индекс.

### `allow_write`

Разрешает изменять исходные файлы.

### `allow_exec`

Разрешает запускать build/tests/tidy и другие разрешённые команды.

---

# 48. `allow_subtasks`

В v2.0 параметр должен существовать как feature flag, но механизм subtasks не является обязательной частью базовой реализации.

Если:

```text
allow_subtasks="NO"
```

агент не создаёт дочерних автономных задач.

Если механизм subtasks будет реализован позже, он должен быть отдельным subsystem и не должен менять основной pipeline.

---

# 49. Conversation Context

Агент должен сохранять контекст текущего диалога.

Однако conversation context не заменяет project index.

Разделение:

```text
Conversation context
    -> что пользователь обсуждает

Project index
    -> что существует в проекте

Filesystem
    -> актуальное содержимое

Build system
    -> как проект реально собирается
```

---

# 50. Context hierarchy

При разрешении запроса используются:

```text
1. explicit user request
2. current conversation context
3. cursor/current file
4. current project
5. indexed project information
6. filesystem/build information
```

При конфликте источников приоритет должен быть явно определён.

---

# 51. Cursor support

Если CLI получает информацию о позиции курсора, она должна быть частью request context.

Например:

```text
file = src/model/model_profile.cpp
line = 42
column = 17
```

Это позволяет разрешать:

```text
"этот класс"
"эта функция"
"кто его вызывает"
```

без угадывания имени.

---

# 52. CLI

Минимальный CLI должен позволять:

```text
fcpp-agent
fcpp-agent open <project>
fcpp-agent ask "<request>"
fcpp-agent build
fcpp-agent test
fcpp-agent status
fcpp-agent projects
```

Точный набор команд может быть расширен.

Важно разделить:

* команды управления агентом;
* команды управления проектом;
* пользовательские AI-запросы.

---

# 53. Интерактивный режим

Должен поддерживаться интерактивный режим:

```text
$ fcpp-agent

fcpp-agent> open ~/src/myproject
fcpp-agent> что делает ModelProfile?
fcpp-agent> кто вызывает load()?
fcpp-agent> исправь эту ошибку
fcpp-agent> build
```

Текущий проект сохраняется в session context.

---

# 54. Non-interactive mode

Должна быть возможность:

```text
fcpp-agent ask "объясни ModelProfile"
```

для использования в shell/scripts.

В non-interactive режиме агент не должен требовать интерактивного подтверждения, если операция изначально безопасна.

Для потенциально изменяющих операций должна быть предусмотрена policy/confirmation.

---

# 55. Ответ агента

Для запросов, где возможна неоднозначность, ответ должен начинаться с краткой интерпретации.

Например:

```text
Понял так: объяснить класс ModelProfile
(model_profile.h); load — его метод.
```

После этого:

```text
...
```

Это является механизмом обнаружения ошибок классификации.

---

# 56. Evidence-first response

LLM, формирующая финальный ответ, должна получать:

```text
User request
+
Resolved intent
+
Evidence
+
Relevant conversation context
```

Она не должна самостоятельно выполнять поиск по проекту.

Например:

```text
Evidence:
    ModelProfile declaration
    methods
    callers
    callees
    source
```

После этого LLM формирует explanation.

---

# 57. Запрет на неподтверждённые утверждения

Для вопросов о проекте агент должен по возможности основывать утверждения на Evidence.

Если доказательств недостаточно:

```text
В индексе недостаточно информации, чтобы уверенно утверждать X.
```

а не:

```text
вероятно X...
```

если это выдаётся как факт.

---

# 58. Работа с отсутствующими данными

Если SQLite не содержит требуемой информации:

```text
SQLite
   ↓ miss
libclang/indexer
   ↓
SQLite update
   ↓
retry query
```

Если получить информацию невозможно:

```text
tool error
   ↓
diagnostic
   ↓
LLM response
```

---

# 59. Ошибки

Ошибки должны быть типизированы.

Минимальные категории:

```cpp
enum class ErrorKind {
    CONFIGURATION,
    PROJECT,
    INDEX,
    COMPILER,
    BUILD,
    TOOL,
    LLM,
    PATCH,
    PERMISSION,
    USER_INPUT
};
```

CLI должен сообщать:

* что произошло;
* на каком этапе;
* можно ли продолжить;
* что пользователь может сделать.

---

# 60. Logging

Агент должен иметь structured/internal logging.

Минимальные уровни:

```text
ERROR
WARN
INFO
DEBUG
TRACE
```

Логи агента не должны попадать в каталог проекта.

---

# 61. Security

Облачный API key не должен попадать:

* в обычный log;
* в diagnostic;
* в prompt;
* в patch;
* в stdout.

Если конфигурация содержит:

```text
model_remote_key="..."
```

это секрет.

---

# 62. Prompt boundaries

В system prompt модели нельзя помещать секреты, не относящиеся к задаче.

API key используется только соответствующим backend.

Project context должен передаваться модели только в необходимом объёме.

---

# 63. Ограничение контекста

Нельзя отправлять в LLM весь проект.

Сначала выполняется:

```text
Intent
→ Subject
→ Evidence Plan
→ Evidence
```

и только релевантные данные передаются модели.

Это является одним из основных преимуществ индексированной архитектуры.

---

# 64. Context budget

Каждый Evidence должен иметь возможность оценить свой размер.

Планировщик должен уметь ограничивать объём данных:

```text
max source lines
max symbols
max callers
max callees
max references
```

При превышении лимита:

```text
top N
+
summary
```

либо другой детерминированный механизм.

---

# 65. Пример полного pipeline

Запрос:

```text
Что делает ModelProfile::load()?
```

### Шаг 1

`lite` определяет:

```text
Area = UNDERSTAND
Subject = METHOD
Name = ModelProfile::load
Relation = SELF
Projection = EXPLANATION
```

### Шаг 2

Resolver находит реальный SymbolId.

### Шаг 3

Effect:

```text
READ_ONLY
```

### Шаг 4

Plan:

```text
outline(ModelProfile)
get_source(load)
callees(load)
```

### Шаг 5

Tools получают Evidence.

### Шаг 6

`main` формирует ответ.

Ответ:

```text
Понял так: объяснить ModelProfile::load().

...
```

---

# 66. Пример запроса callers

Запрос:

```text
Кто вызывает load()?
```

Intent:

```text
Area       = UNDERSTAND
Subject    = METHOD
Relation   = CALLERS
Projection = LOCATIONS
Effect     = READ_ONLY
```

Plan:

```text
resolve load()
callers(load)
```

LLM получает список callers и формирует ответ.

---

# 67. Пример изменения

Запрос:

```text
Переименуй ModelProfile::load в read
```

Intent:

```text
Area       = CHANGE
ChangeKind = MODIFY
Subject    = METHOD
Relation   = SELF
Projection = SOURCE
Effect     = WRITE_SOURCE
```

Но фактический plan должен дополнительно учитывать references:

```text
resolve method
refs(method)
get_source(affected files)
```

LLM генерирует patch.

После этого:

```text
validate patch
apply patch
```

После изменения желательно выполнить релевантную проверку:

```text
compile
```

или другую проверку, если она предусмотрена policy.

---

# 68. Пример VERIFY

Запрос:

```text
Собери проект.
```

Intent:

```text
Area = VERIFY
Projection = ...
Effect = EXECUTE
```

Plan:

```text
compile_obj / build
```

LLM не должна сама придумывать:

```text
make
gcc ...
```

если проект является CMake/Ninja project.

---

# 69. Пример REVIEW

Запрос:

```text
Проверь этот класс на проблемы потокобезопасности.
```

Intent:

```text
Area       = REVIEW
Subject    = CLASS
Relation   = SELF
Projection = EXPLANATION
Effect     = READ_ONLY
```

Plan:

```text
outline
source
relevant fields
relevant methods
call graph where necessary
```

Затем LLM выполняет reasoning над собранными данными.

---

# 70. Пример неоднозначности

Пользователь:

```text
Исправь load().
```

Если найдено:

```text
ModelProfile::load()
Config::load()
Database::load()
```

агент не должен выбирать случайный метод и писать код.

Допустимые варианты:

```text
Какой load() имеется в виду:
1. ModelProfile::load()
2. Config::load()
3. Database::load()
```

либо, если текущий cursor однозначно указывает на `ModelProfile::load()`:

```text
Понял так: ModelProfile::load().
```

---

# 71. Benchmark

Необходимо создать сквозной benchmark.

Каждая запись должна содержать:

```text
user phrase
cursor fixture
project fixture
expected Area
expected Subject
expected Relation
expected Projection
expected Effect
expected tools
required answer properties
```

Например:

```text
Input:
    "кто вызывает эту функцию"

Cursor:
    tests/foo.cpp:17:5

Expected:
    Area = UNDERSTAND
    Relation = CALLERS
    Projection = LOCATIONS
    Effect = READ_ONLY
    Tool = callers
```

---

# 72. Benchmark является живым набором

Каждый обнаруженный реальный промах должен становиться новой benchmark case.

Таким образом:

```text
bug
 ↓
reproduce
 ↓
benchmark case
 ↓
fix
 ↓
regression test
```

Это обязательный процесс развития классификатора.

---

# 73. Acceptance criteria для Intent

Intent resolver считается реализованным, если:

1. типовые запросы правильно классифицируются;
2. Subject разрешается через индекс;
3. ambiguity не приводит к случайному write;
4. Effect вычисляется deterministic core;
5. выбранный tool соответствует Intent;
6. ошибочная интерпретация видна пользователю;
7. существующие benchmark cases не деградируют.

---

# 74. Acceptance criteria для index

Индекс считается работоспособным, если:

1. проект можно открыть;
2. index создаётся вне проекта;
3. SQLite сохраняется между запусками;
4. второй запуск не требует полной переиндексации без причины;
5. изменённые файлы обнаруживаются;
6. можно получить основные C++ relationships;
7. отсутствие записи приводит к fallback на indexer;
8. индекс можно обновить без удаления всего проекта.

---

# 75. Acceptance criteria для LLM abstraction

Backend считается корректно абстрагированным, если:

1. core agent не знает API конкретного provider;
2. можно заменить provider конфигурацией;
3. `same_as_main` работает;
4. llama-cpp работает для локальной модели;
5. OpenAI-compatible backend не требует изменения core;
6. добавление нового backend не требует изменения Intent/Planner/Tools.

---

# 76. Acceptance criteria для tools

Каждый tool должен:

* иметь typed request;
* иметь typed result;
* быть отдельно тестируемым;
* не зависеть от LLM;
* возвращать диагностируемую ошибку;
* использовать ProjectContext;
* иметь определённый Effect.

---

# 77. Acceptance criteria для patching

Изменение считается успешным только если:

```text
patch generated
    ↓
patch validated
    ↓
policy passed
    ↓
patch applied
```

При ошибке на любом этапе:

```text
source remains unchanged
```

за исключением специально предусмотренной атомарной операции.

---

# 78. Архитектурные ограничения

Нельзя:

1. делать LLM единственным источником информации о проекте;
2. заставлять LLM угадывать пути к файлам;
3. заставлять LLM свободно выбирать системные tools;
4. хранить служебные файлы агента в проекте;
5. смешивать SQLite schema с LLM prompt logic;
6. жёстко связывать core с конкретным LLM API;
7. использовать giant enum для всех пользовательских запросов;
8. выполнять write/exec только потому, что LLM так решила;
9. применять неподтверждённый patch;
10. запускать произвольные shell-команды для обычного редактирования исходников.

---

# 79. Расширяемость

Архитектура должна позволять независимо добавлять:

### Новый compiler backend

```text
CompilerBackend
    + Clang22Backend
    + ...
```

### Новый LLM provider

```text
LlmProvider
    + LlamaCpp
    + OpenAICompat
    + Claude
    + ...
```

### Новый tool

```text
Tool
    + NewTool
```

### Новый Relation

```text
Relation
```

### Новый Projection

```text
Projection
```

### Новый Area

Добавление новой области не должно требовать переписывания всех tools.

---

# 80. Что не входит в обязательный scope v2.0

Следующие возможности могут быть предусмотрены архитектурно, но не являются обязательными для первой реализации:

* автономные subtasks;
* свободный agentic tool calling;
* несколько одновременно работающих моделей;
* автоматический выбор между большим количеством remote models;
* web search;
* универсальный shell agent;
* поддержка языков кроме C++;
* IDE integration;
* GUI;
* распределённый индекс;
* облачное хранение project context.

---

# 81. Рекомендуемая структура исходного кода

Конкретные имена файлов могут быть изменены, но архитектурное разделение должно сохраняться.

Пример:

```text
src/
    main.cpp

    cli/
        cli.hpp
        cli.cpp
        repl.hpp
        repl.cpp

    config/
        config.hpp
        config.cpp

    project/
        project.hpp
        project.cpp
        project_manager.hpp
        project_manager.cpp
        project_context.hpp
        project_context.cpp

    index/
        index.hpp
        sqlite_index.hpp
        sqlite_index.cpp
        clang_indexer.hpp
        clang_indexer.cpp

    llm/
        llm_provider.hpp
        llama_cpp.hpp
        llama_cpp.cpp
        openai_compat.hpp
        openai_compat.cpp
        claude.hpp
        claude.cpp

    intent/
        intent.hpp
        intent_analyzer.hpp
        intent_analyzer.cpp
        subject_resolver.hpp
        subject_resolver.cpp

    planner/
        evidence_plan.hpp
        planner.hpp
        planner.cpp

    tools/
        tool.hpp
        tool_registry.hpp
        tool_registry.cpp
        get_source.cpp
        outline.cpp
        callers.cpp
        callees.cpp
        overrides.cpp
        refs.cpp
        includes.cpp
        compile_obj.cpp
        run_tests.cpp
        tidy.cpp
        apply_patch.cpp

    patch/
        patch.hpp
        patch_validator.cpp
        patch_applier.cpp

    build/
        build_system.hpp
        cmake_backend.cpp
        ninja_backend.cpp

    context/
        conversation.hpp
        evidence.hpp

    policy/
        policy.hpp
        policy.cpp

    benchmark/
        ...
```

Это пример архитектурного разбиения, а не обязательная файловая структура.

---

# 82. Основной цикл агента

Основной application loop должен концептуально выглядеть так:

```cpp
while (running) {
    UserRequest request = cli.read_request();

    RequestContext context =
        context_manager.prepare(request);

    Intent intent =
        intent_analyzer.analyze(context);

    ResolvedIntent resolved =
        resolver.resolve(intent, context);

    PolicyDecision policy =
        policy.check(resolved);

    if (!policy.allowed()) {
        cli.report(policy);
        continue;
    }

    EvidencePlan plan =
        planner.make(resolved);

    Evidence evidence =
        executor.collect(plan);

    AgentResponse response =
        response_model.generate(
            request,
            resolved,
            evidence);

    cli.print(response);
}
```

Для `CHANGE` добавляется отдельная ветка:

```text
Intent
 ↓
Evidence
 ↓
Patch generation
 ↓
Patch validation
 ↓
Policy
 ↓
Apply
 ↓
Verify
```

---

# 83. Важнейшее правило архитектуры

LLM должна отвечать на вопрос:

> **«Что пользователь хочет?»**

Deterministic core должен отвечать на вопрос:

> **«Как безопасно и точно получить данные/выполнить эту задачу?»**

LLM не должна подменять deterministic core.

---

# 84. Реализационная последовательность

Рекомендуемый порядок реализации:

## Этап 1. Skeleton

Реализовать:

* CMake;
* CLI;
* config;
* ProjectContext;
* logging;
* basic REPL.

---

## Этап 2. Project/index

Реализовать:

* ProjectManager;
* SQLite;
* schema versioning;
* clang indexer;
* project opening;
* index warming.

---

## Этап 3. C++ query layer

Реализовать:

* symbols;
* source;
* outline;
* callers;
* callees;
* refs;
* overrides;
* includes.

На этом этапе LLM ещё может отсутствовать.

---

## Этап 4. Tools

Реализовать Tool Registry и deterministic execution.

---

## Этап 5. Intent

Реализовать:

```text
Area
Subject
Relation
Projection
Effect
```

и `Intent`.

---

## Этап 6. LLM abstraction

Реализовать:

* LlamaCpp;
* model roles;
* same_as;
* structured output.

---

## Этап 7. Planner

Связать:

```text
Intent
→ EvidencePlan
→ Tools
```

---

## Этап 8. Response generation

Подключить LLM для формирования ответа на основании Evidence.

---

## Этап 9. Patch workflow

Реализовать:

* patch generation;
* validation;
* apply;
* build verification.

---

## Этап 10. Benchmark

Создать набор regression cases.

---

# 85. Минимальный MVP

Минимально полезная версия должна уметь:

```text
open project
    ↓
build index
    ↓
ask:
    "что делает этот класс?"
    "кто вызывает эту функцию?"
    "где используется этот символ?"
    "что вызывает эта функция?"
    ↓
получить структурированный Intent
    ↓
получить Evidence из SQLite/libclang
    ↓
получить ответ LLM
```

После этого:

```text
изменить код
    ↓
generate patch
    ↓
validate
    ↓
apply
    ↓
build
```

---

# 86. Definition of Done

`fcpp-agent v2.0` считается реализованным, если выполнены все следующие условия:

### Infrastructure

* [ ] работает на FreeBSD;
* [ ] предусмотрена Linux support;
* [ ] используется CMake;
* [ ] используется Ninja;
* [ ] clang 22 является основным backend;
* [ ] compiler abstraction не привязан к версии 22.

### Project

* [ ] проект открывается;
* [ ] проект можно переключить;
* [ ] project state сохраняется;
* [ ] служебные данные находятся вне проекта.

### Index

* [ ] используется libclang;
* [ ] SQLite является persistent index;
* [ ] индекс прогревается при открытии;
* [ ] существует incremental update;
* [ ] отсутствующие данные запрашиваются через indexer.

### LLM

* [ ] существует общий LLM API;
* [ ] есть `lite`;
* [ ] есть `main`;
* [ ] предусмотрен `remote`;
* [ ] работает `same_as_main`;
* [ ] llama-cpp поддерживается;
* [ ] API backend расширяем.

### Intent

* [ ] реализован Area;
* [ ] реализован Subject;
* [ ] реализован Relation;
* [ ] реализован Projection;
* [ ] реализован Effect;
* [ ] реализован deterministic subject resolution;
* [ ] реализована ambiguity handling.

### Tools

* [ ] существует Tool Registry;
* [ ] реализованы основные C++ tools;
* [ ] tools не зависят от LLM;
* [ ] planner детерминирован.

### Changes

* [ ] LLM генерирует patch;
* [ ] patch валидируется;
* [ ] patch применяется контролируемо;
* [ ] write policy соблюдается;
* [ ] после изменения возможна verification.

### Quality

* [ ] существует benchmark;
* [ ] реальные ошибки классификации превращаются в regression cases;
* [ ] benchmark запускается автоматически;
* [ ] изменение core не должно незаметно ухудшать предыдущие cases.

---

# 87. Ключевой критерий качества проекта

Качество `fcpp-agent` нельзя оценивать только по тому, насколько хорошо LLM пишет C++.

Основной критерий:

> **насколько надёжно агент превращает естественный запрос разработчика в правильную, проверяемую и безопасную последовательность действий над реальным C++ проектом.**

Система должна стремиться не к:

```text
LLM knows everything
```

а к:

```text
LLM understands intent
        ↓
deterministic resolution
        ↓
indexed evidence
        ↓
deterministic tools
        ↓
LLM reasoning
        ↓
controlled change
        ↓
verification
```

---

# 88. Принцип «нулевых промахов» не является требованием

Система не обязана гарантировать отсутствие ошибок интерпретации.

Вместо этого обязательны три свойства.

### Ошибка должна быть видна

Интерпретация запроса отображается пользователю.

### Цена ошибки должна быть ограничена

При сомнении выбирается менее опасный Effect.

### Ошибка должна быть измерима

Каждый реальный failure превращается в benchmark case.

Именно benchmark является механизмом постепенного улучшения классификации.

---

# 89. Agent API

## 89.1. Назначение

`fcpp-agent` является headless-сервисом, предоставляющим единый API для взаимодействия с внешними клиентами:

* CLI;
* code-OSS;
* Sublime Text;
* CLion;
* другими IDE/редакторами;
* потенциально другими инструментами автоматизации.

CLI не является архитектурным центром системы. Он должен использовать тот же Agent API, что и GUI-клиенты.

API предоставляет две принципиально разные группы операций:

1. **управление агентом и проектом** — открыть проект, получить состояние, переиндексировать проект, запустить build/test и т.п.;
2. **работа с запросами пользователя** — передать текст запроса вместе с текущим контекстом редактора и получить ответ агента.

API не должен раскрывать клиентам внутреннюю реализацию агента.

В частности, клиенту не требуется знать:

* `Area`;
* `Relation`;
* `Projection`;
* внутренний `Intent`;
* Evidence Plan;
* конкретные инструменты `get_source`, `callers`, `callees` и т.д.;
* используемую LLM;
* способ хранения SQLite cache;
* способ получения данных через libclang.

Эти сущности являются внутренними компонентами Agent Core.

---

## 89.2. Архитектурная схема

```text
+------------------+
|     code-OSS     |
+------------------+
          |
+------------------+
|     Sublime      |
+------------------+
          |
+------------------+
|      CLion       |
+------------------+
          |
+------------------+
|       CLI        |
+------------------+
          |
          v
+---------------------------+
|        Agent API          |
|  protocol / validation    |
|  sessions / requests      |
|  events / errors          |
+---------------------------+
          |
          v
+---------------------------+
|        Agent Core         |
|                           |
| Intent / Planner          |
| Subject Resolver          |
| Evidence Planner          |
| Tool Registry             |
| LLM Router                |
+---------------------------+
          |
    +-----+-----+
    |           |
    v           v
+--------+   +---------+
| SQLite |   | libclang|
+--------+   +---------+
```

Transport является отдельным слоем.

Конкретный transport может быть заменён без изменения Agent API и Agent Core.

Предпочтительный первоначальный вариант transport:

* локальный Unix domain socket;
* JSON-RPC 2.0 поверх этого соединения.

В дальнейшем допускается добавление:

* TCP;
* HTTP;
* WebSocket;
* другого транспорта.

При этом семантика методов и сообщений не должна зависеть от transport.

---

# 89.3. Идентификаторы

API использует три разных идентификатора.

## 89.3.1. `project_id`

Идентифицирует открытый проект.

`project_id` не должен совпадать с filesystem path.

Например:

```text
project_id = "project-7f31"
path       = "/home/user/src/my-project"
```

Это позволяет в дальнейшем изменить механизм идентификации проектов без изменения API.

Один `project_id` соответствует одному открытому проекту внутри экземпляра агента.

---

## 89.3.2. `session_id`

Идентифицирует клиентскую рабочую сессию.

Session не равна project.

Один проект может одновременно использоваться несколькими клиентами:

```text
project A
   |
   +-- session IDE-1
   |
   +-- session IDE-2
   |
   +-- session CLI
```

Каждая session имеет собственный:

* текущий editor context;
* контекст диалога;
* историю запросов;
* состояние отмены текущих операций.

Session может переключаться между проектами.

---

## 89.3.3. `request_id`

Идентифицирует одну операцию API.

`request_id` обязателен для всех потенциально долгих операций.

Примеры:

```text
request_id = "req-01H..."
```

Один request имеет одно терминальное состояние.

Request не следует путать с message/dialog turn.

Например:

```text
session_id = "sess-123"

request_id = "req-001"
request = "Почему здесь используется virtual?"

request_id = "req-002"
request = "Покажи callers этого метода"
```

---

# 89.4. Общий формат сообщения

Все API-сообщения имеют JSON-представление.

Общие поля:

```json
{
  "id": "req-001",
  "method": "project.open",
  "params": {}
}
```

Для ответа:

```json
{
  "id": "req-001",
  "result": {}
}
```

Для ошибки:

```json
{
  "id": "req-001",
  "error": {
    "code": "PROJECT_NOT_FOUND",
    "message": "Project directory does not exist",
    "details": {}
  }
}
```

Для событий:

```json
{
  "event": "request.progress",
  "request_id": "req-001",
  "project_id": "project-123",
  "session_id": "sess-001",
  "data": {}
}
```

---

# 89.5. Версионирование API

API имеет явную версию.

Например:

```text
fcpp-agent API v1
```

Версия не должна зависеть от версии LLM, clang или внутренней базы данных.

Начальная версия:

```text
api_version = 1
```

Изменения, несовместимые с существующими клиентами, требуют увеличения major version.

Новые необязательные поля и новые методы могут добавляться без изменения major version.

---

# 89.6. Жизненный цикл Agent

Состояние самого агента:

```text
STARTING
READY
STOPPING
STOPPED
ERROR
```

### `STARTING`

Агент запускается:

* читает configuration;
* открывает/создаёт SQLite databases;
* восстанавливает состояние проектов;
* подготавливает providers;
* поднимает API.

### `READY`

Агент принимает API requests.

### `STOPPING`

Агент прекращает принимать новые операции и завершает допустимые текущие операции.

### `STOPPED`

API больше не обслуживается.

### `ERROR`

Агент не способен нормально выполнять операции.

---

# 89.7. Agent methods

Начальный API должен включать следующие методы.

## 7.1. `agent.status`

Получить состояние агента.

Request:

```json
{
  "id": "req-001",
  "method": "agent.status",
  "params": {}
}
```

Response:

```json
{
  "id": "req-001",
  "result": {
    "api_version": 1,
    "state": "READY",
    "version": "0.1.0",
    "projects": 2
  }
}
```

---

# 89.8. Project API

## 89.8.1. `project.open`

Открыть проект.

Request:

```json
{
  "id": "req-002",
  "method": "project.open",
  "params": {
    "path": "/home/user/src/my-project"
  }
}
```

Response:

```json
{
  "id": "req-002",
  "result": {
    "project_id": "project-7f31",
    "path": "/home/user/src/my-project",
    "state": "OPENING"
  }
}
```

Открытие проекта является потенциально долгой операцией.

Поэтому фактическое состояние открытия передаётся через events.

После открытия:

```json
{
  "event": "project.state_changed",
  "project_id": "project-7f31",
  "data": {
    "state": "READY"
  }
}
```

При открытии проекта агент должен:

1. проверить path;
2. определить project root;
3. найти или создать соответствующую SQLite cache;
4. загрузить состояние проекта;
5. проверить актуальность индекса;
6. при необходимости инициировать index update;
7. сделать проект доступным для requests.

---

## 89.8.2. `project.close`

Закрыть проект.

```json
{
  "id": "req-003",
  "method": "project.close",
  "params": {
    "project_id": "project-7f31"
  }
}
```

Response:

```json
{
  "id": "req-003",
  "result": {
    "project_id": "project-7f31",
    "state": "CLOSED"
  }
}
```

Закрытие проекта не удаляет cache.

---

## 89.8.3. `project.status`

```json
{
  "id": "req-004",
  "method": "project.status",
  "params": {
    "project_id": "project-7f31"
  }
}
```

Response:

```json
{
  "id": "req-004",
  "result": {
    "project_id": "project-7f31",
    "path": "/home/user/src/my-project",
    "state": "READY",
    "index": {
      "state": "READY",
      "generation": 42
    }
  }
}
```

---

# 89.9. Index API

Индексирование должно иметь две разные операции:

* incremental refresh;
* full rebuild.

Их нельзя смешивать.

## 89.9.1. `index.status`

```json
{
  "id": "req-010",
  "method": "index.status",
  "params": {
    "project_id": "project-7f31"
  }
}
```

Response:

```json
{
  "id": "req-010",
  "result": {
    "state": "READY",
    "generation": 42,
    "files": 1834,
    "symbols": 27152,
    "stale_files": 3
  }
}
```

---

## 89.9.2. `index.refresh`

Инкрементальное обновление.

```json
{
  "id": "req-011",
  "method": "index.refresh",
  "params": {
    "project_id": "project-7f31"
  }
}
```

Агент определяет изменившиеся файлы и обновляет соответствующие части cache.

---

## 89.9.3. `index.rebuild`

Полная перестройка cache.

```json
{
  "id": "req-012",
  "method": "index.rebuild",
  "params": {
    "project_id": "project-7f31"
  }
}
```

Операция:

1. создаёт новую generation индекса;
2. индексирует проект;
3. проверяет результат;
4. атомарно делает новую generation активной.

При ошибке старая валидная generation должна оставаться доступной.

Это важно: неудачный rebuild не должен уничтожить работоспособный cache.

---

# 89.10. Session API

## 89.10.1. `session.open`

```json
{
  "id": "req-020",
  "method": "session.open",
  "params": {
    "project_id": "project-7f31"
  }
}
```

Response:

```json
{
  "id": "req-020",
  "result": {
    "session_id": "sess-91ab",
    "project_id": "project-7f31"
  }
}
```

---

## 89.10.2. `session.close`

```json
{
  "id": "req-021",
  "method": "session.close",
  "params": {
    "session_id": "sess-91ab"
  }
}
```

---

# 89.11. Editor Context

Редакторский контекст является частью API, но не является обязательным.

Это принципиально важно.

Проект может быть открыт в IDE, но:

* файл не открыт;
* активная вкладка отсутствует;
* курсор отсутствует;
* редактор не имеет текущего source file.

Поэтому request должен работать и без editor context.

Структура:

```json
{
  "file": {
    "path": "src/foo.cpp"
  },
  "cursor": {
    "line": 41,
    "column": 17
  },
  "selection": {
    "start": {
      "line": 40,
      "column": 5
    },
    "end": {
      "line": 43,
      "column": 12
    }
  }
}
```

Все поля editor context, кроме `file.path` при наличии context, являются необязательными.

---

# 89.12. `session.update_context`

IDE должна иметь возможность сообщать агенту текущий editor context до отправки пользовательского запроса.

```json
{
  "id": "req-030",
  "method": "session.update_context",
  "params": {
    "session_id": "sess-91ab",
    "editor": {
      "file": {
        "path": "src/foo.cpp"
      },
      "cursor": {
        "line": 41,
        "column": 17
      }
    }
  }
}
```

После этого:

```text
ask
```

может использовать сохранённый context.

---

# 89.13. `ask`

Основной пользовательский метод.

```json
{
  "id": "req-040",
  "method": "ask",
  "params": {
    "session_id": "sess-91ab",
    "project_id": "project-7f31",
    "text": "Почему здесь используется virtual?",
    "editor": {
      "file": {
        "path": "src/foo.cpp"
      },
      "cursor": {
        "line": 41,
        "column": 17
      }
    }
  }
}
```

`editor` в request имеет приоритет над сохранённым context session.

Если `editor` отсутствует, используется context session.

Если context session отсутствует, запрос выполняется без editor context.

---

# 89.14. Ответ `ask`

Для простого случая:

```json
{
  "id": "req-040",
  "result": {
    "state": "COMPLETED",
    "answer": {
      "text": "Метод объявлен virtual, потому что..."
    }
  }
}
```

Однако для длительных операций ответ `ask` должен рассматриваться как acknowledgement начала операции, а окончательный результат передаётся отдельным событием.

Рекомендуемый response:

```json
{
  "id": "req-040",
  "result": {
    "request_id": "req-040",
    "state": "RUNNING"
  }
}
```

Затем клиент получает события:

```text
request.started
request.interpretation
request.progress
request.completed
```

---

# 89.15. Request State Machine

Каждый request имеет состояние.

```text
QUEUED
   |
   v
RUNNING
   |
   +----------+
   |          |
   v          v
COMPLETED   FAILED
   |
   v
CANCELLED
```

Более точно:

```text
QUEUED
  |
  +--> CANCELLED
  |
  v
RUNNING
  |
  +--> COMPLETED
  |
  +--> FAILED
  |
  +--> CANCELLED
```

Терминальные состояния:

```text
COMPLETED
FAILED
CANCELLED
```

После перехода в terminal state request не может быть снова запущен.

---

# 89.16. Request phases

Для `ask` желательно предоставлять фазу выполнения:

```text
QUEUED
ANALYZING
RESOLVING_SUBJECT
PLANNING
COLLECTING_EVIDENCE
GENERATING
VALIDATING
APPLYING
COMPLETED
```

Не все запросы проходят все phases.

Например, read-only request:

```text
QUEUED
ANALYZING
RESOLVING_SUBJECT
PLANNING
COLLECTING_EVIDENCE
GENERATING
COMPLETED
```

Запрос на изменение:

```text
QUEUED
ANALYZING
RESOLVING_SUBJECT
PLANNING
COLLECTING_EVIDENCE
GENERATING
VALIDATING
APPLYING
COMPLETED
```

---

# 89.17. Events

API должен поддерживать server-to-client events.

Event имеет общий формат:

```json
{
  "event": "request.progress",
  "request_id": "req-040",
  "session_id": "sess-91ab",
  "project_id": "project-7f31",
  "timestamp": "2026-10-06T12:30:15Z",
  "data": {}
}
```

`timestamp` используется для диагностики и отображения progress.

---

# 89.18. Основные события

## `request.started`

```json
{
  "event": "request.started",
  "request_id": "req-040",
  "session_id": "sess-91ab",
  "project_id": "project-7f31",
  "data": {}
}
```

---

## `request.interpretation`

Позволяет клиенту видеть, как агент понял запрос.

Например:

```json
{
  "event": "request.interpretation",
  "request_id": "req-040",
  "data": {
    "area": "UNDERSTAND",
    "subject": {
      "kind": "FUNCTION",
      "display": "Foo::bar()"
    },
    "relation": "SELF",
    "projection": "EXPLANATION",
    "effect": "READ_ONLY"
  }
}
```

Это не является обязательным внутренним API для GUI, но рекомендуется предоставлять событие для прозрачности работы агента.

---

# 89.19. `request.progress`

Общее событие прогресса:

```json
{
  "event": "request.progress",
  "request_id": "req-040",
  "data": {
    "phase": "COLLECTING_EVIDENCE",
    "message": "Collecting callers",
    "completed": 3,
    "total": 5
  }
}
```

`completed/total` могут отсутствовать, если количество операций неизвестно.

---

# 89.20. Index events

### `index.started`

```json
{
  "event": "index.started",
  "project_id": "project-7f31",
  "request_id": "req-012",
  "data": {
    "mode": "REBUILD"
  }
}
```

### `index.progress`

```json
{
  "event": "index.progress",
  "project_id": "project-7f31",
  "request_id": "req-012",
  "data": {
    "files_processed": 850,
    "files_total": 1834
  }
}
```

### `index.completed`

```json
{
  "event": "index.completed",
  "project_id": "project-7f31",
  "request_id": "req-012",
  "data": {
    "generation": 43,
    "files": 1834,
    "symbols": 27301
  }
}
```

### `index.failed`

```json
{
  "event": "index.failed",
  "project_id": "project-7f31",
  "request_id": "req-012",
  "data": {
    "error": {
      "code": "INDEX_FAILED",
      "message": "..."
    }
  }
}
```

---

# 89.21. Tool events

Внутренние tools могут быть отражены в progress events.

Например:

```json
{
  "event": "tool.started",
  "request_id": "req-040",
  "data": {
    "tool": "callers",
    "subject": "Foo::bar()"
  }
}
```

и:

```json
{
  "event": "tool.completed",
  "request_id": "req-040",
  "data": {
    "tool": "callers",
    "result_count": 7
  }
}
```

Однако клиент не должен зависеть от конкретного набора tools.

Tools являются внутренней реализацией.

---

# 89.22. LLM events

Допускаются:

```text
llm.started
llm.completed
llm.failed
```

Например:

```json
{
  "event": "llm.started",
  "request_id": "req-040",
  "data": {
    "role": "main"
  }
}
```

Название конкретной модели не обязательно должно передаваться клиенту.

Это позволяет менять provider/model configuration без изменения client protocol.

---

# 89.23. Финальные события request

## `request.completed`

```json
{
  "event": "request.completed",
  "request_id": "req-040",
  "session_id": "sess-91ab",
  "project_id": "project-7f31",
  "data": {
    "answer": {
      "text": "Метод объявлен virtual, потому что..."
    }
  }
}
```

---

## `request.failed`

```json
{
  "event": "request.failed",
  "request_id": "req-040",
  "data": {
    "error": {
      "code": "LLM_UNAVAILABLE",
      "message": "Main model is unavailable",
      "retryable": true
    }
  }
}
```

---

## `request.cancelled`

```json
{
  "event": "request.cancelled",
  "request_id": "req-040",
  "data": {}
}
```

---

# 89.24. Отмена операции

Для всех длительных операций должен существовать:

```text
request.cancel
```

Request:

```json
{
  "id": "req-050",
  "method": "request.cancel",
  "params": {
    "request_id": "req-040"
  }
}
```

Response:

```json
{
  "id": "req-050",
  "result": {
    "request_id": "req-040",
    "state": "CANCELLED"
  }
}
```

Отмена является cooperative cancellation.

Каждый внутренний компонент, участвующий в request execution, должен по возможности проверять cancellation state.

---

# 89.25. Build API

Agent API также предоставляет прямые команды, не проходящие через LLM.

Это необходимо для IDE и CLI.

Минимальный набор:

```text
build.configure
build.build
build.clean
tests.run
tidy.run
```

Например:

```json
{
  "id": "req-060",
  "method": "build.build",
  "params": {
    "project_id": "project-7f31",
    "configuration": "Debug"
  }
}
```

Агент возвращает request_id и публикует progress events.

---

# 89.26. Прямые команды и `ask`

API намеренно разделяет:

```text
ask
```

и:

```text
project.*
index.*
build.*
tests.*
tidy.*
```

`ask` является LLM-assisted операцией.

Прямые команды являются deterministic API operations.

Например:

```text
"полностью переиндексируй проект"
```

не должно обязательно проходить через LLM.

IDE может непосредственно вызвать:

```text
index.rebuild
```

Это:

* быстрее;
* детерминированнее;
* проще для GUI;
* безопаснее;
* не требует расхода LLM tokens.

---

# 89.27. Error model

Все ошибки имеют единый формат:

```json
{
  "code": "ERROR_CODE",
  "message": "Human readable message",
  "retryable": false,
  "details": {}
}
```

`code` является машинно-обрабатываемым идентификатором.

`message` предназначено для человека.

`details` содержит структурированную дополнительную информацию.

---

# 89.28. Базовые error codes

## API

```text
INVALID_REQUEST
INVALID_PARAMS
METHOD_NOT_FOUND
UNSUPPORTED_API_VERSION
```

## Agent

```text
AGENT_NOT_READY
AGENT_SHUTTING_DOWN
INTERNAL_ERROR
```

## Project

```text
PROJECT_NOT_FOUND
PROJECT_ALREADY_OPEN
PROJECT_NOT_OPEN
INVALID_PROJECT_PATH
PROJECT_BUSY
```

## Session

```text
SESSION_NOT_FOUND
SESSION_PROJECT_MISMATCH
```

## Request

```text
REQUEST_NOT_FOUND
REQUEST_ALREADY_FINISHED
REQUEST_CANCELLED
REQUEST_BUSY
```

## Index

```text
INDEX_NOT_READY
INDEX_FAILED
INDEX_BUSY
INDEX_CORRUPTED
```

## Build

```text
BUILD_FAILED
BUILD_NOT_CONFIGURED
BUILD_BUSY
```

## LLM

```text
LLM_UNAVAILABLE
LLM_TIMEOUT
LLM_INVALID_RESPONSE
LLM_PROVIDER_ERROR
```

## Tools

```text
TOOL_NOT_FOUND
TOOL_FAILED
TOOL_TIMEOUT
```

## Permissions

```text
READ_NOT_ALLOWED
WRITE_NOT_ALLOWED
EXEC_NOT_ALLOWED
BUILD_WRITE_NOT_ALLOWED
```

---

# 89.29. Error example

```json
{
  "id": "req-071",
  "error": {
    "code": "PROJECT_NOT_OPEN",
    "message": "Project is not open",
    "retryable": false,
    "details": {
      "project_id": "project-7f31"
    }
  }
}
```

---

# 89.30. Request/Project/Session relationship

Нормальная схема:

```text
Agent
 |
 +-- Project A
 |     |
 |     +-- Session 1
 |     |      |
 |     |      +-- Request 1
 |     |      +-- Request 2
 |     |
 |     +-- Session 2
 |            |
 |            +-- Request 3
 |
 +-- Project B
       |
       +-- Session 3
```

Request должен ссылаться на session и project.

Это позволяет однозначно определить:

* над каким проектом работает request;
* какой editor context используется;
* какой conversation context используется;
* кому отправлять events.

---

# 89.31. Request context snapshot

При начале `ask` агент должен создать snapshot контекста.

Snapshot включает как минимум:

```text
project_id
session_id
editor context
conversation context
index generation
agent configuration relevant to request
```

Это необходимо, чтобы длительный request не менял смысл посередине выполнения.

Например:

```text
t0:
index generation = 42

t1:
ask started

t2:
IDE запускает index.rebuild

t3:
index generation = 43
```

Текущий request продолжает работать с зафиксированным контекстом generation 42, если конкретный tool не предусматривает иной семантики.

---

# 89.32. Conversation context

Conversation context принадлежит `session_id`.

Он не должен быть глобальным для проекта.

Например:

```text
Project A
 |
 +-- Session IDE-1
 |      conversation A
 |
 +-- Session IDE-2
        conversation B
```

Это предотвращает смешивание контекста двух пользователей/IDE sessions.

---

# 89.33. Concurrency

Несколько read-only requests могут выполняться параллельно.

Операции, изменяющие project state, могут требовать serialisation.

Минимальные правила:

* `ask` READ_ONLY может выполняться параллельно;
* `index.refresh` не должен одновременно конфликтовать с `index.rebuild`;
* `index.rebuild` должен иметь exclusive access к активируемой generation;
* `build` операции должны учитывать состояние build directory;
* WRITE_SOURCE requests должны сериализоваться относительно конфликтующих изменений;
* один request не может одновременно иметь два состояния execution.

Конкретная модель scheduler является внутренней реализацией.

---

# 89.34. Idempotency

Для потенциально повторяемых административных операций API должен по возможности обеспечивать idempotent semantics.

Например:

```text
project.open
```

для уже открытого проекта должен вернуть существующий `project_id`, если path соответствует тому же проекту.

Повторный:

```text
index.refresh
```

не должен приводить к повреждению cache.

`index.rebuild` при этом является отдельной операцией и может создавать новую generation.

---

# 89.35. API discovery

Для облегчения интеграции IDE рекомендуется предоставить:

```text
agent.capabilities
```

Response:

```json
{
  "api_version": 1,
  "methods": [
    "agent.status",
    "project.open",
    "project.close",
    "project.status",
    "index.status",
    "index.refresh",
    "index.rebuild",
    "session.open",
    "session.close",
    "session.update_context",
    "ask",
    "request.cancel",
    "build.configure",
    "build.build",
    "build.clean",
    "tests.run",
    "tidy.run"
  ],
  "events": [
    "request.started",
    "request.interpretation",
    "request.progress",
    "request.completed",
    "request.failed",
    "request.cancelled",
    "index.started",
    "index.progress",
    "index.completed",
    "index.failed"
  ]
}
```

Это позволяет GUI определить возможности конкретной версии агента.

---

# 89.36. Minimal GUI integration

Минимальный IDE plugin не обязан реализовывать всю модель Agent API.

Для базового сценария достаточно:

```text
1. connect
2. agent.status
3. project.open
4. session.open
5. session.update_context
6. ask
7. receive events
8. request.cancel
9. session.close
```

При каждом изменении active editor plugin может отправлять:

```text
session.update_context
```

При нажатии пользователем "Ask":

```text
ask
```

с текстом запроса.

---

# 89.37. Example: IDE request lifecycle

Пользователь находится в:

```text
src/foo.cpp:41:17
```

и пишет:

```text
Почему этот метод virtual?
```

IDE отправляет:

```json
{
  "id": "req-100",
  "method": "ask",
  "params": {
    "session_id": "sess-1",
    "project_id": "project-1",
    "text": "Почему этот метод virtual?",
    "editor": {
      "file": {
        "path": "src/foo.cpp"
      },
      "cursor": {
        "line": 41,
        "column": 17
      }
    }
  }
}
```

Agent:

```text
request.started
        |
        v
request.interpretation
        |
        v
COLLECTING_EVIDENCE
        |
        +--> tool.started(get_source)
        |
        +--> tool.completed(get_source)
        |
        +--> tool.started(overrides)
        |
        +--> tool.completed(overrides)
        |
        v
llm.started
        |
        v
llm.completed
        |
        v
request.completed
```

IDE при этом не знает, какие именно tools использовал агент.

---

# 89.38. Example: project administration

CLI или IDE хочет полностью переиндексировать проект.

Вместо:

```text
ask("полностью переиндексируй проект")
```

клиент вызывает:

```json
{
  "id": "req-200",
  "method": "index.rebuild",
  "params": {
    "project_id": "project-1"
  }
}
```

Получает:

```json
{
  "id": "req-200",
  "result": {
    "request_id": "req-200",
    "state": "RUNNING"
  }
}
```

и затем:

```text
index.started
index.progress
index.progress
...
index.completed
```

---

# 89.39. API boundary

Следующие сущности являются частью внешнего API:

```text
Agent
Project
Session
Request
EditorContext
RequestState
OperationPhase
Event
Error
```

Следующие сущности являются внутренними:

```text
Intent
Area
Subject
Relation
Projection
Effect
EvidencePlan
Tool
SQLite schema
libclang implementation
LLM provider
model routing
```

Это важная архитектурная граница.

Изменение внутренней модели классификации запроса не должно требовать изменения IDE plugin.

---

# 89.40. Безопасность API

Agent API предполагается локальным API.

По умолчанию он не должен принимать внешние подключения из сети.

Для Unix socket необходимо обеспечить filesystem permissions, позволяющие подключаться только пользователю, запустившему agent, либо явно разрешённой группе.

Особенно защищёнными являются операции:

```text
WRITE_SOURCE
EXECUTE
WRITE_BUILD
```

API permission model должна использовать те же ограничения, что и внутренний Agent Core.

Клиент не может повысить permission через API request.

Например:

```json
{
  "allow_write": false
}
```

нельзя переопределить через:

```text
ask
```

или:

```text
apply_patch
```

---

# 41. Transport independence

Внутренний API должен быть определён в терминах abstract messages:

```cpp
struct ApiRequest;
struct ApiResponse;
struct ApiEvent;
```

Transport layer отвечает только за:

```text
serialize
deserialize
send
receive
connection
```

Agent Core не должен зависеть от:

```text
Unix socket
JSON-RPC
HTTP
WebSocket
```

Это позволит позже добавить другой transport без изменения core logic.

---

# 89.42. C++ API representation

Внутри C++ рекомендуется иметь типизированное представление:

```cpp
using ProjectId = std::string;
using SessionId = std::string;
using RequestId = std::string;

struct EditorPosition {
    std::size_t line{};
    std::size_t column{};
};

struct EditorContext {
    std::filesystem::path file;
    std::optional<EditorPosition> cursor;
    std::optional<EditorSelection> selection;
};

struct AskRequest {
    RequestId request_id;
    ProjectId project_id;
    SessionId session_id;
    std::string text;
    std::optional<EditorContext> editor;
};

enum class RequestState {
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled
};
```

Конкретный JSON representation не должен проникать в Agent Core глубже API adapter layer.

---

# 89.43. Recommended internal API layers

```text
Transport
    |
    v
Protocol Adapter
    |
    v
Agent API Service
    |
    +-- ProjectService
    +-- SessionService
    +-- RequestService
    +-- IndexService
    +-- BuildService
    |
    v
Agent Core
```

Например:

```text
JSON-RPC
   |
   v
JsonRpcServer
   |
   v
AgentApi
   |
   v
RequestService
   |
   v
Intent / Planner / Tools / LLM
```

---

# 89.44. API invariant

Главный архитектурный инвариант:

> Любой клиент, способный говорить с Agent API, должен иметь возможность использовать `fcpp-agent` без знания его внутренней реализации.

Следовательно:

* IDE не знает про libclang;
* IDE не знает про SQLite;
* IDE не знает про `Area`;
* IDE не знает про `Relation`;
* IDE не выбирает tools;
* CLI не имеет особых полномочий;
* GUI и CLI используют одинаковый API;
* изменение LLM provider не требует изменения клиента;
* изменение внутреннего planner не требует изменения клиента.

---

# 89.45. Initial API surface

Для первой реализации достаточно следующего набора:

```text
agent.status
agent.capabilities

project.open
project.close
project.status

index.status
index.refresh
index.rebuild

session.open
session.close
session.update_context

ask
request.cancel

build.configure
build.build
build.clean

tests.run
tidy.run
```

Расширение API выполняется добавлением новых методов, не нарушающих существующие semantics.

---

# 89.46. Definition of Done для Agent API

Agent API считается реализованным, если:

1. CLI работает через Agent API, а не напрямую через Agent Core.
2. Один и тот же API может использовать IDE plugin.
3. Поддерживаются `project_id`, `session_id`, `request_id`.
4. Проект и session являются разными сущностями.
5. `ask` работает с cursor context.
6. `ask` работает без cursor context.
7. Editor context может передаваться непосредственно в request.
8. Editor context может обновляться отдельно через `session.update_context`.
9. Доступны прямые administrative commands.
10. `index.refresh` и `index.rebuild` различаются.
11. Долгие операции имеют state.
12. Долгие операции публикуют progress events.
13. Requests можно отменить.
14. Ошибки имеют стабильные machine-readable codes.
15. API не раскрывает внутренние `Area/Relation/Projection/Tool` как обязательные client-side concepts.
16. Transport можно заменить без изменения Agent Core.
17. API имеет versioning.
18. API является безопасным для локального использования.
19. Ошибка rebuild не уничтожает последнюю валидную index generation.
20. Поведение CLI и IDE не расходится по semantics.

---

# 89.47. Architectural conclusion

`fcpp-agent` следует рассматривать не как CLI с дополнительным GUI, а как **headless C++ agent service с API**.

CLI, IDE и другие клиенты являются равноправными API clients:

```text
                 +----------------+
                 |    code-OSS    |
                 +----------------+
                         |
+----------------+       |
|    Sublime     |-------+
+----------------+       |
                         |
+----------------+       |
|     CLion      |-------+
+----------------+       |
                         |
+----------------+       |
|      CLI       |-------+
+----------------+       |
                         v
                 +----------------+
                 |  Agent API     |
                 +----------------+
                         |
                 +----------------+
                 |  Agent Core    |
                 +----------------+
                         |
              +----------+----------+
              |                     |
        +-----------+         +-----------+
        |  SQLite   |         |  libclang |
        +-----------+         +-----------+
```

Это является базовой архитектурной границей системы.

CLI при этом становится всего лишь одним из клиентов Agent API, а не специальным интерфейсом Agent Core.

---

# 90. Итоговая модель

Архитектура `fcpp-agent` должна строиться вокруг следующей цепочки:

```text
                 USER
                  |
                  v
             User Request
                  |
                  v
             LLM / lite
                  |
                  v
                Intent
                  |
        +---------+---------+
        |                   |
        v                   v
     Subject            Policy
     Resolver
        |
        v
 Resolved Intent
        |
        v
 Deterministic Planner
        |
        v
   Evidence Plan
        |
        v
   Tool Registry
        |
        +----------+----------+
        |          |          |
        v          v          v
      SQLite    libclang    Build
        |          |          |
        +----------+----------+
                   |
                   v
                Evidence
                   |
                   v
              LLM / main
                   |
             +-----+-----+
             |           |
             v           v
           Answer      Patch
                         |
                         v
                    Validation
                         |
                         v
                      Policy
                         |
                         v
                       Apply
                         |
                         v
                     Verify
```

Это является основной архитектурной моделью `fcpp-agent v2.0`.

---

# 91. Решения, оставленные открытыми

Следующие вопросы **не следует молча решать в рамках реализации**, если они не будут дополнительно определены заказчиком:

1. Точный набор CLI-команд.
2. Точная SQLite schema.
3. Формат хранения project mapping.
4. Полный список C++ сущностей индекса.
5. Алгоритм определения stale index.
6. Точный механизм получения `compile_commands.json`.
7. Формат structured output конкретных LLM.
8. Точные модели/параметры llama-cpp.
9. Требования к streaming.
10. Механизм user confirmation для write/exec.
11. Полный список разрешённых executable operations.
12. Поведение при отсутствии clang-tidy.
13. Точный алгоритм context budgeting.
14. Реализация `remote`.
15. Реализация subtasks.
16. Точная политика автоматического build после patch.
17. Требования к производительности индекса.
18. Максимальный размер проекта, который должен поддерживаться.
19. Политика SQLite schema решена: новые БД создаются по baseline, существующие строго проверяются без миграций.
20. Требования к параллельной работе нескольких CLI sessions.

До принятия этих решений реализация должна использовать интерфейсы и default behavior, не блокирующие дальнейшее развитие системы.

---

# 92. Главный architectural invariant

При дальнейшем развитии проекта необходимо сохранять следующий invariant:

```text
Natural Language
      ↓
LLM interpretation
      ↓
Small stable Intent
      ↓
Deterministic resolution
      ↓
Deterministic evidence plan
      ↓
Typed tools
      ↓
Evidence
      ↓
LLM reasoning/generation
```

Нельзя превращать систему обратно в:

```text
Natural Language
      ↓
LLM
      ↓
"сам реши, какой tool вызвать и что сделать"
```

Именно сохранение этого разделения является основным архитектурным требованием `fcpp-agent v2.0`.

---

# 93. Уточнение (принято): CLI — тонкий клиент Agent API

**Статус:** ✅ Принято.

Фиксируется обязательное требование: CLI (`fcpp-agent`) **не обращается к Agent Core напрямую**. CLI является равноправным клиентом Agent API (§89): каждая команда CLI транслируется в соответствующий метод Agent API (`agent.*`, `project.*`, `index.*`, `session.*`, `ask`, `build.*`, `tests.*`, `tidy.*`), а не вызывает внутренние компоненты напрямую.

Следствия для реализации:
- Agent API первичен; CLI, GUI и IDE — его потребители с одинаковой семантикой (§89.44, §89.47).
- Любая команда CLI обязана иметь эквивалентный метод Agent API и совпадать с ним по семантике.
- До появления процесса Agent API CLI может запускать Agent Core **in-process**, но исключительно через тот же API-слой (`AgentApi`/services). Прямые вызовы core-компонентов из CLI запрещены.
- Текущая реализация (Этапы 1–3) нарушает это требование: `main.cpp` вызывает core напрямую.
- Устранение — Этап 8 дорожной карты ([`tasks/todo.md`](tasks/todo.md)); до его завершения отклонение считается известным техническим долгом.

---

# 94. Уточнение (принято): минимальный контракт запроса `ask`

**Статус:** ✅ Принято.

Клиент Agent API (CLI/IDE/GUI) при пользовательском запросе передаёт **только**:

```text
text    — текст запроса
editor  — опциональный контекст: { file { path }, cursor { line, column } }
```

*(Ссылки: ADR-14, §89.11, §89.13.)*

Требования:

1. `file` и `cursor` **необязательны**; запрос обязан работать без них (§89.11).
2. Клиент **не знает и не передаёт** внутренние сущности ядра: `Area`, `Relation`, `Projection`, `Subject`, `Tool`, `target`. Их выводит детерминированное ядро (Intent + план).
3. `ask` является методом Agent API (`EApiMethod::Ask`); CLI обязан вызывать его через `AgentApi` (§93).
4. `--stream` — режим вывода, **не** часть тела запроса.
5. Команда `route --area/--relation/--projection` **удаляется** из публичной (клиентской) поверхности как раскрывающая внутренние enum (§89.39/§90).
6. Вид субъекта (`m_kind`) определяется из текста запроса; курсор лишь локализует сущность запрошенного вида (§21, ADR-13).

---

# 95. Уточнение (принято): граница клиента — три поля

**Статус:** ✅ Принято. (Уточняет §94; ADR-21.)

Клиент (IDE/плагин/CLI) знает и передаёт **только**:

```text
text    — текст запроса (всегда)
file    — файл (опционально; может отсутствовать, напр. IDE только открыта)
cursor  — позиция курсора (опционально; осмысленна только вместе с file)
```

Требования:

1. `file` и `cursor` необязательны; запрос работает без них (§89.11).
2. Всё прочее (`session_id`, `project_id`, `request_id`, события `request.*`/`index.*`, состояния запроса) — **прозрачная маршрутизация агента**, а не знание клиента.
3. Случаи: нет `file` → общий вопрос (контекст из индекса проекта); `file` без `cursor` → вопрос про файл; `file`+`cursor` → вопрос про точку (символ под курсором, ADR-20/B2).
4. Это единственное обязательное знание IDE; оно совпадает с телом `ask` (§94).
