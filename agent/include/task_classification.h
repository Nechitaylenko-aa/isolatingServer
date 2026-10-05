#pragma once
#include <string>

namespace cppagent {

// Слой 0: Нужен ли курсор этой задаче вообще
enum class CursorBinding {
    NOT_NEEDED,       // задача не про конкретное место: архитектура, общий вопрос, новый файл
    NEEDED_PRESENT,   // задача привязана к месту, и курсор (file/line/col) есть
    NEEDED_MISSING,   // задача привязана к месту, но курсор пуст/невалиден
};

// Слой 1: Детализация задачи — что конкретно хочет юзер
enum class Detail {
    // --- UNDERSTAND: графовые запросы ---
    SIMPLE_READ,              // "покажи метод", "что здесь" — только locate
    FIND_IMPLEMENTATIONS,     // "найди все реализации" — override-ы
    ANALYZE_INTERFACE,        // "проверь интерфейс на консистентность"
    CALL_CHAIN,               // "кто вызывает этот метод" — upstream callers
    CALL_GRAPH_SUBTREE,       // "что вызывает этот метод" — downstream callees
    DEPENDENCY_MAP,           // "что зависит от этого класса" — include-граф
    DATA_FLOW,                // "откуда берётся это значение" — trace источника
    TYPE_USAGE,               // "где используется этот тип"
    LIFECYCLE_TRACE,          // "когда создаётся/уничтожается объект"
    
    // --- UNDERSTAND: сравнительные ---
    COMPARE_IMPLS,            // "в чём разница между двумя реализациями"
    DIFF_INTERFACE_IMPL,      // "что в интерфейсе не покрыто реализацией"
    
    // --- UNDERSTAND: виртуальные симуляции (reasoning без правок) ---
    VIRTUAL_REFACTOR_SIM,     // "можно ли заменить эти два метода одним"
    FEASIBILITY_CHECK,        // "можно ли перенести метод в другой класс"
    
    // --- UNDERSTAND: общие вопросы ---
    GENERAL_QUESTION,         // "как работает подсистема X" — CursorBinding=NOT_NEEDED
    
    // --- SANDBOX_FIX: локальные правки ---
    LOCAL_FIX,                // правка в одном методе/файле
    
    // --- SANDBOX_FIX: правки сигнатуры (каскадные) ---
    RENAME,                   // переименование с обновлением всех ссылок
    SIGNATURE_CHANGE,         // изменить параметры/return — каскад
    ADD_OVERRIDE,             // добавить реализацию pure virtual в наследнике
    
    // --- SANDBOX_FIX: структурный рефакторинг ---
    EXTRACT_METHOD,           // вытащить кусок в отдельный метод
    INLINE_METHOD,            // инлайнить тело в каждый call site
    EXTRACT_CLASS,            // разбить класс на два
    MOVE_METHOD,              // перенести метод в другой класс
    
    // --- SANDBOX_FIX: удаление ---
    DEAD_CODE_REMOVAL,        // убрать недостижимый код
    REMOVE_UNUSED_INCL,       // почистить лишние #include
    
    // --- SANDBOX_FIX: добавление ---
    ADD_METHOD,               // объявить + реализовать новый метод
    ADD_CLASS,                // создать новый класс/структуру
    
    // --- QUARRY_DESIGN: design-задачи ---
    NEW_INTERFACE,            // спроектировать абстрактный интерфейс с нуля
    API_DESIGN,               // спроектировать публичный API класса/модуля
    APPLY_PATTERN,            // применить паттерн (strategy, factory, observer)
    SPLIT_CLASS,              // декомпозиция God Object
    DESIGN_HIERARCHY,         // спроектировать иерархию наследования
    
    // --- TEST_GEN ---
    UNIT_TEST,                // юнит-тесты на конкретный метод
    INTEGRATION_TEST,         // интеграционные тесты на подсистему
    GEN_MOCKS,                // сгенерировать моки/стабы
    COVERAGE_GAP,             // найти непокрытые тестами ветки
    
    // --- EXPERIMENT ---
    PROTOTYPE,                // набросать прототип
    BENCHMARK,                // измерить производительность
    TRY_ALTERNATIVE,          // предложить альтернативную реализацию
    
    // --- INFRA ---
    BUILD_FIX,                // починить сломанную сборку
    BUILD_CONFIG,             // изменить CMake/build-систему
    DEPS_UPDATE,              // обновить/добавить зависимость
    CI_PIPELINE,              // настройка CI/CD
    
    // --- Качество и долг ---
    CODE_SMELL,               // "почему этот код плох"
    DUPLICATE_FIND,           // "где дубликаты этого куска"
    COMPLEXITY_HOTSPOT,       // "самые сложные места в модуле"
    TODO_SCAN,                // собрать все TODO/FIXME/HACK
    
    // --- Документация ---
    DOC_GEN,                  // сгенерировать doc-комментарии
    DOC_CHECK,                // проверить актуальность доков
    DOC_SUMMARY,              // краткое описание подсистемы
    
    // --- Миграция / модернизация ---
    MODERNIZE,                // переписать на новый стандарт (auto, concepts)
    API_MIGRATION,            // перевести со старого API на новый
    PORT_PLATFORM,            // адаптация под другую платформу
    
    // --- Безопасность ---
    SECURITY_AUDIT,           // "проверь на уязвимости"
    INPUT_VALIDATION,         // "все ли входы валидируются"
    THREADING_SAFE,           // "потокобезопасно ли это"
    
    // --- META ---
    EXCEEDS_LOCAL_AI,         // задача превышает возможности локальной AI
};

// Слой 2: Где мы физически находимся в коде
enum class CursorContext {
    NO_CONTEXT,                  // файл не открыт, курсор нулевой
    PURE_VIRTUAL_METHOD,         // метод = 0 в интерфейсе
    VIRTUAL_METHOD_WITH_BODY,    // virtual с реализацией в базе
    CONCRETE_METHOD_IMPL,        // обычная реализация метода (override или нет)
    NON_VIRTUAL_METHOD,          // метод класса, не virtual
    PIMPL_IMPL,                  // реализация в .cpp (Impl-класс pimpl)
    INTERFACE_CLASS_DECL,        // курсор на объявлении класса (class X {})
    CC_FOREST,                   // между классами в .h, или до/после класса
    PLAIN_HEADER,                // .h с enum/struct/typedef
    FREE_FUNCTION,               // свободная функция, не метод
};

// Слой 3: Как место относится к задаче
enum class Relevance {
    ENTRY_POINT_EXACT,           // курсор точно на том, что просили
    ENTRY_POINT_NEARBY,          // курсор в правильном файле/классе, но не на точной строке
    UNRELATED,                   // курсор вообще не там
    AMBIGUOUS,                   // не можем решить без юзера
};

// Слой 4: Что конкретно делать
enum class Action {
    // --- Категория A: реализовано ---
    QUERY_LOCATE_ONLY,           // просто показать сигнатуру
    QUERY_FIND_IMPLEMENTORS,     // найти реализации через наследование
    QUERY_VIRTUAL_OVERRIDES,     // найти overrides через USR
    QUERY_CALL_CHAIN,            // кто вызывает метод (входящие рёбра calls)
    QUERY_CALL_SUBTREE,          // кого вызывает метод (исходящие рёбра calls)
    QUERY_DEPENDENCY_MAP,        // кто подключает файл / что подключает он сам (tu_dep)
    QUERY_TODO_SCAN,             // найти TODO/FIXME/HACK/XXX (детерминированно)
    QUERY_DOC_CHECK,             // проверить наличие doc-комментариев (детерминированно)
    QUERY_CODE_SMELL,            // эвристика качества по телу метода
    QUERY_TYPE_USAGE,            // где используется тип (refs по USR класса)
    QUERY_COMPLEXITY,            // хотспоты сложности: длина/вложенность/строки
    QUERY_DUPLICATES,            // методы с одинаковой сигнатурой (кандидаты в дубли)
    
    // --- Категория B: архитектурно готово, query не написан ---
    NOT_IMPLEMENTED_YET,         // Detail признан, но query/механизм ещё не готов
    
    // --- Категория C: превышает возможности локальной AI ---
    DELEGATE_TO_MAIN,            // делегировать в main-модель (будущее)
    REFUSE_TOO_BROAD,            // отказать явно (задача слишком широкая)
    
    // --- Обработка нестыковок ---
    NEEDS_INPUT_RELEVANCE,       // Relevance==UNRELATED/AMBIGUOUS -> спросить
    NEEDS_INPUT_CURSOR,          // CursorBinding==NEEDED_MISSING -> запросить курсор
};

// Утилиты для преобразования enum -> string
std::string detailToString(Detail d);
std::string cursorContextToString(CursorContext c);
std::string cursorBindingToString(CursorBinding b);
std::string relevanceToString(Relevance r);
std::string actionToString(Action a);

// Обратное преобразование (для тестов/десериализации)
Detail detailFromString(const std::string& s, bool& ok);

// Слой 3: относится ли ФИЗИЧЕСКОЕ место под курсором (ctx) к тому, что просил
// юзер (detail + goal_text). goalText/enclosingMethodUsr необязательны: без них
// суждение деградирует до чисто позиционного (метод vs класс vs no-context).
// enclosingMethodUsr нужен, чтобы отличить "задача про ЭТОТ метод" от "задача
// явно про ДРУГОЙ метод" — детерминированная проверка по имени.
Relevance computeRelevance(Detail detail, CursorContext ctx,
                           const std::string& goalText = "",
                           const std::string& enclosingMethodUsr = "");

// Слой 4: Таблица решений. Выбирает Action по (Primitive, Detail, CursorContext),
// где relevance — МОДИФИКАТОР: UNRELATED/AMBIGUOUS форсит NEEDS_INPUT_RELEVANCE
// независимо от таблицы (arch.md: слой 3 -> слой 4).
// primitiveStr передаётся строкой, чтобы не тянуть зависимость на orchestrator.h.
Action selectAction(const std::string& primitiveStr, Detail detail, CursorContext ctx,
                    Relevance relevance = Relevance::ENTRY_POINT_EXACT);

} // namespace cppagent