#include "model_broker.h"
#include "task_classification.h"
#include <algorithm>
#include <cctype>
#include "json.hpp"

namespace cppagent {

using json = nlohmann::json;

// --- helpers for Primitive string mapping ---

std::string primitiveToString(Primitive p) {
    switch(p){
        case Primitive::UNDERSTAND: return "UNDERSTAND";
        case Primitive::SANDBOX_FIX: return "SANDBOX_FIX";
        case Primitive::QUARRY_DESIGN: return "QUARRY_DESIGN";
        case Primitive::TEST_GEN: return "TEST_GEN";
        case Primitive::EXPERIMENT: return "EXPERIMENT";
        case Primitive::INFRA: return "INFRA";
    }
    return "UNDERSTAND";
}
namespace {
// "sandbox_fix" / "Sandbox-Fix" / "SANDBOX FIX" / "SANDBOX_FIX " -> "SANDBOXFIX"
std::string normalizePrimitiveKey(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (c == '_' || c == '-' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '.') continue;
        out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}
} // anon

// Терпимый разбор: регистр и разделители не важны. Модель на temp 0.7 регулярно
// отдаёт "sandbox_fix" вместо "SANDBOX_FIX", и раньше это молча превращалось
// в unavailable() -> принудительный UNDERSTAND в оркестраторе.
Primitive primitiveFromString(const std::string& s, bool& ok){
    struct Entry { const char* key; Primitive p; };
    static const Entry kTable[] = {
        {"UNDERSTAND",     Primitive::UNDERSTAND},
        {"SANDBOXFIX",     Primitive::SANDBOX_FIX},
        {"QUARRYDESIGN",   Primitive::QUARRY_DESIGN},
        {"TESTGEN",        Primitive::TEST_GEN},
        {"EXPERIMENT",     Primitive::EXPERIMENT},
        {"INFRA",          Primitive::INFRA},
        {"INFRASTRUCTURE", Primitive::INFRA},
    };
    const std::string key = normalizePrimitiveKey(s);
    for (const auto& e : kTable)
        if (key == e.key) { ok = true; return e.p; }
    ok = false;
    return Primitive::UNDERSTAND;
}

namespace {

// std::tolower в локали "C" не понижает кириллицу (UTF-8 многобайтный проходит
// как есть): "Проверь" не находилось по ключу "проверь". Понижаем А-Я и Ё вручную.
std::string toLower(std::string s) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { out += (char)std::tolower(c); i += 1; continue; }
        if (i + 1 < s.size()) {
            unsigned char c2 = (unsigned char)s[i+1];
            if (c == 0xD0 && c2 >= 0x90 && c2 <= 0x9F) { out += (char)0xD0; out += (char)(c2 + 0x20); i += 2; continue; }
            if (c == 0xD0 && c2 >= 0xA0 && c2 <= 0xAF) { out += (char)0xD1; out += (char)(c2 - 0x20); i += 2; continue; }
            if (c == 0xD0 && c2 == 0x81) { out += (char)0xD1; out += (char)0x91; i += 2; continue; }
            if (c == 0xD1) { out.append(s, i, 2); i += 2; continue; }
        }
        out += (char)c; i += 1;
    }
    return out;
}
bool containsAny(const std::string& text, std::initializer_list<const char*> needles) {
    for(auto* n: needles) if(text.find(n)!=std::string::npos) return true;
    return false;
}
// Stub теперь заполняет и Detail (слой 1): раньше все возвраты оставляли
// Detail::GENERAL_QUESTION, из-за чего selectAction/computeRelevance
// вырождались и слои 1/3/4 нельзя было проверить офлайн.
PrimitiveGuess makeGuess(Primitive p, std::string rationale, Detail d){
    PrimitiveGuess g{p, primitiveToString(p), std::move(rationale), false};
    g.detail = d;
    g.detailStr = detailToString(d);
    return g;
}
PrimitiveGuess unavailable(std::string reason){
    return PrimitiveGuess{Primitive::UNDERSTAND, "", std::move(reason), true};
}

} // anon

// Stub: детерминированная эвристика по 6 примитивам (заменится Light)
PrimitiveGuess StubModelBroker::classifyIntent(const std::string& goalText) {
    std::string t = toLower(goalText);
    // INFRA — сборка/окружение, без правки логики (высокий приоритет)
    if(containsAny(t, {"пересобери","пересобрать","обнови compile","собери проект","инфра"})) {
        // уточняем что это не "добавь cmake таргет" (это QUARRY)
        if(!containsAny(t, {"добавь","создай","новый"}))
            return makeGuess(Primitive::INFRA, "команда обслуживания окружения/сборки без правки логики", Detail::BUILD_CONFIG);
    }
    // INFRA по ключевым словам сборки (если нет явных добавлений)
    if(containsAny(t, {"cmake","compile_commands","граф зависимост","build_graph","граф таргет","граф сборк","ninja","make "}) &&
       containsAny(t, {"покажи","обнови","собери","пересобери"})) {
        return makeGuess(Primitive::INFRA, "запрос информации об окружении/сборке", Detail::BUILD_CONFIG);
    }
    // INFRA: read-only запросы про сборку/таргеты
    if(containsAny(t, {"покажи граф","граф build","таргет","target","зависимост сборк"}) &&
       !containsAny(t, {"метод","класс","функци","символ"})) {
        return makeGuess(Primitive::INFRA, "запрос информации о графе сборки/таргетах", Detail::BUILD_CONFIG);
    }
    // EXPERIMENT — гипотеза с откатом
    if(containsAny(t, {"попробу","а что если","что если","эксперимент","гипотеза","посмотрим","скомпилируется ли","откат","undo","redo"}))
        return makeGuess(Primitive::EXPERIMENT, "формулировка гипотезы/эксперимента с возможным откатом", Detail::TRY_ALTERNATIVE);
    // TEST_GEN — цель == тесты
    if(containsAny(t, {"напиши тест","покрой тест","покрыть тест","юнит-тест","unit test","google test","gtest","тест для"})){
        // "попробуй переписать и покрой тестами" -> не TEST_GEN
        if(containsAny(t, {"перепиши","исправь","замени","отрефактор","добавь класс"}))
            ; // fall through to FIX/DESIGN
        else
            return makeGuess(Primitive::TEST_GEN, "явная просьба написать/покрыть тестами", Detail::UNIT_TEST);
    }
    // UNDERSTAND — вопрос без просьбы менять код
    bool isQuestion = containsAny(t, {"что делает","объясни","где ","найди где","как работает","почему","утечка","логическ","покажи","выведи","отобрази","clang_visit","проверь","убедись","проанализируй","оцени","дойди до","разберись"});
    bool wantsChange = containsAny(t, {"исправь","почини","замени","отрефактор","добавь","создай","вынеси","перепиши","оптимизир"});
    if(isQuestion && !wantsChange)
        return makeGuess(Primitive::UNDERSTAND, "вопрос/объяснение без просьбы менять код на диске", Detail::SIMPLE_READ);
    // QUARRY_DESIGN — новые файлы/модули/архитектура
    if(containsAny(t, {"новый класс","новый модуль","новый файл","добавь брокер","скелет плагина","вынеси логику","отдельный сервис","архитектур","воркфлоу","workflow","нескольк компонент","много класс"}))
        return makeGuess(Primitive::QUARRY_DESIGN, "просьба создать новые модули/файлы или изменить архитектуру", Detail::API_DESIGN);
    // SANDBOX_FIX — точечная правка под курсором
    if(containsAny(t, {"этот метод","эту функцию","в этом методе","в этом классе","в методе","в функции","исправь баг","выход за границ","сырой указатель","unique_ptr","исправь"}))
        return makeGuess(Primitive::SANDBOX_FIX, "точечная правка одного метода/класса под курсором", Detail::LOCAL_FIX);
    // Правила разрешения конфликтов из ТЗ
    if(wantsChange) {
        // широкое vs точечное
        if(containsAny(t, {"весь класс","весь модуль","фреймворк","сервис"}))
            return makeGuess(Primitive::QUARRY_DESIGN, "конфликт отрефакторить+широкий контекст -> QUARRY_DESIGN", Detail::API_DESIGN);
        return makeGuess(Primitive::SANDBOX_FIX, "конфликт по умолчанию для правки одного места -> SANDBOX_FIX", Detail::LOCAL_FIX);
    }
    // дефолт: не распознали — считаем вопросом, безопасно
    return makeGuess(Primitive::UNDERSTAND, "явных сигналов правки нет — трактуем как UNDERSTAND (read-only безопасно)", Detail::GENERAL_QUESTION);
}

// --- LightModelBroker: классификация через IApiAI (провайдер-агностик) ---
// extractFirstJsonObject и HTTP живут в api_ai (CApiOpenAI). Здесь — только
// сборка промпта и семантическая интерпретация ответа.

LightModelBroker::LightModelBroker(std::string baseUrl, std::string modelName, double temperature)
    : api_(std::make_shared<CApiOpenAI>(std::move(baseUrl), modelName)),
      modelName_(std::move(modelName)), temperature_(temperature) {}

PrimitiveGuess LightModelBroker::classifyIntent(const std::string& goalText){
    const std::string systemPrompt =
        "Ты — детерминированный системный шлюз C++ агента. Твоя ЕДИНСТВЕННАЯ задача — перевести техническое задание пользователя в один из 6 системных примитивов.\n"
        "ОТВЕЧАЙ СТРОГО ВАЛИДНЫМ JSON БЕЗ MARKDOWN: {\"primitive\": \"UNDERSTAND\"|\"SANDBOX_FIX\"|\"QUARRY_DESIGN\"|\"TEST_GEN\"|\"EXPERIMENT\"|\"INFRA\", \"detail\": \"<см. ниже>\", \"cursor_binding\": \"NOT_NEEDED\"|\"NEEDED_PRESENT\"|\"NEEDED_MISSING\", \"confidence\": 0.0..1.0, \"alternative\": \"второй по вероятности примитив или пустая строка\", \"goal_specific\": true|false, \"rationale\": \"одна фраза по-русски\"}\n"
        "confidence — НАСКОЛЬКО ТЫ УВЕРЕН в выбранном primitive (0.0 = совсем не уверен, 1.0 = абсолютно уверен). Будь ЧЕСТЕН: на неоднозначных фразах ставь 0.4-0.6 и указывай alternative, не завышай.\n"
        "goal_specific — ДРУГОЕ измерение, не confidence. Даже при 100% уверенности, что это ПРАВКА, отдельно спроси себя: есть ли в goal_text конкретный объект (метод/файл/поведение) и понятно ли, когда правка 'готова'?\n"
        "goal_specific=false — просьба без объекта/критерия: 'сделай лучше', 'почини это', 'оптимизируй', 'наведи порядок' — без указания ЧТО и КАК. goal_specific=true — указан конкретный метод/класс/файл/симптом: 'переименуй getUserName в getUsername', 'исправь утечку в set_pass_hash'. Для UNDERSTAND/INFRA почти всегда true.\n"
        "detail — уточнение ВНУТРИ primitive (ЧТО именно хочет юзер). Допустимые значения по primitive:\n"
        "- UNDERSTAND: SIMPLE_READ (показать/что делает ЭТОТ метод), FIND_IMPLEMENTATIONS (все реализации), CALL_CHAIN (кто вызывает), CALL_GRAPH_SUBTREE (кого вызывает), DEPENDENCY_MAP (кто подключает файл / от чего зависит: \"от чего зависит\", \"кто включает заголовок\", \"какие файлы тянут класс\"), DATA_FLOW (откуда берётся значение/трассировка потока), TYPE_USAGE (\"где используется тип/класс\", \"кто использует класс/тип\", \"найди использования класса\" — вопрос про ИСПОЛЬЗОВАНИЯ, а не обзор), LIFECYCLE_TRACE (когда создаётся/уничтожается объект), ANALYZE_INTERFACE, COMPARE_IMPLS (сравни две реализации / в чём разница), DIFF_INTERFACE_IMPL, VIRTUAL_REFACTOR_SIM/FEASIBILITY_CHECK (можно ли ... без правок), DOC_SUMMARY (кратко опиши словами что делает подсистема/класс), DOC_GEN (сгенерируй doc-комментарии), DOC_CHECK (проверь наличие doc-комментариев), TODO_SCAN (найди TODO/FIXME/HACK/XXX), CODE_SMELL (почему этот код плох / запахи / пахнет), DUPLICATE_FIND (где дубликаты / одинаковые методы), COMPLEXITY_HOTSPOT (самые сложные места / хотспоты), SECURITY_AUDIT, INPUT_VALIDATION, THREADING_SAFE, MODERNIZE, API_MIGRATION, PORT_PLATFORM, GENERAL_QUESTION (ОБЗОР/архитектура: \"как устроен класс/модуль/подсистема в целом\", \"что это за класс\", \"опиши архитектуру\" — НЕ про конкретный метод и НЕ про поиск использований).\n"
        "ВАЖНО про граф вызовов (НЕ путать): 'кто вызывает X' / 'кто использует X' / 'откуда вызывается X' -> CALL_CHAIN. 'кого вызывает X' / 'что вызывает X' / 'что внутри X вызывает' -> CALL_GRAPH_SUBTREE.\n"
        "- SANDBOX_FIX: LOCAL_FIX, RENAME, SIGNATURE_CHANGE, ADD_OVERRIDE, EXTRACT_METHOD, INLINE_METHOD, EXTRACT_CLASS, MOVE_METHOD, DEAD_CODE_REMOVAL, REMOVE_UNUSED_INCL, ADD_METHOD, ADD_CLASS.\n"
        "- QUARRY_DESIGN: NEW_INTERFACE, API_DESIGN, APPLY_PATTERN, SPLIT_CLASS, DESIGN_HIERARCHY.\n"
        "- TEST_GEN: UNIT_TEST, INTEGRATION_TEST, GEN_MOCKS, COVERAGE_GAP.\n"
        "- EXPERIMENT: PROTOTYPE, BENCHMARK, TRY_ALTERNATIVE.\n"
        "- INFRA: BUILD_FIX, BUILD_CONFIG, DEPS_UPDATE, CI_PIPELINE.\n"
        "Сомневаешься в detail — GENERAL_QUESTION для чтения, LOCAL_FIX для правки.\n"
        "cursor_binding — привязана ли задача к конкретному МЕСТУ в коде: NOT_NEEDED (общий вопрос об архитектуре/подсистеме, новый файл с нуля — курсор не нужен), NEEDED_PRESENT (задача про конкретный метод/класс/строку), NEEDED_MISSING (задача явно про конкретное место, но какое — не названо).\n"
        "КРИТЕРИИ:\n"
        "1. UNDERSTAND — вопрос/объяснение/поиск/ПРОВЕРКА. Код на диске НЕ меняется. Примеры: \"Что делает этот метод?\", \"Где утечка?\", \"Объясни clang_visitChildren\", \"Проверь корректность реализаций\", \"Дойди до реализации и убедись\". Вопросы о МЕСТЕ проблемы без глагола изменения (\"где утечка\", \"где баг\", \"где ошибка\", \"где теряется\") — UNDERSTAND, даже если названы 'класс'/'метод'/'модуль'.\n"
        "ГЛАВНОЕ ПРАВИЛО ГЛАГОЛА: примитив определяется ГЛАГОЛОМ цели, а не упомянутыми именами методов/классов.\n"
        "- Читающие глаголы: \"проверь\", \"убедись\", \"проанализируй\", \"оцени\", \"дойди до\", \"разберись\", \"найди\", \"посмотри\", \"объясни\" — БЕЗ глагола изменения -> UNDERSTAND, даже если названы конкретные методы, классы, файлы и реализацию надо \"дойти\".\n"
        "- Глаголы изменения: \"исправь\", \"замени\", \"добавь\", \"перепиши\", \"отрефактори\", \"оптимизируй\", \"почини\", \"удали\" -> write-примитив.\n"
        "- Если в цели сказано \"без правок\", \"не меняй\", \"ничего не менять\", \"только проверь\" — это ЖЁСТКО UNDERSTAND.\n"
        "Не путай: \"Проверь, корректно ли реализован метод X\" = UNDERSTAND (надо прочитать/проанализировать), НЕ SANDBOX_FIX. Названные имена методов сами по себе НЕ делают задачу правкой.\n"
        "2. SANDBOX_FIX — починить/отрефакторить ОДИН метод/класс под курсором. Локально. Примеры: \"Замени на unique_ptr в этом методе\", \"Исправь выход за границы\"\n"
        "3. QUARRY_DESIGN — новая сущность/файлы/архитектура/воркфлоу >1 компонента. Требует ветки. Примеры: \"Добавь брокер для OpenAI\", \"Вынеси AST в сервис\"\n"
        "4. TEST_GEN — цель == тесты (написать/покрыть/добавить тесты). Примеры: \"Напиши тесты для compile_commands.cpp на gtest\", \"Покрой тестами этот модуль, нужен gtest\", \"Напиши юнит-тест\". Слова \"покрой тестами\", \"покрыть тестами\", \"gtest\", \"юнит-тест\" — приоритет TEST_GEN, если нет глагола эксперимента/правки логики.\n"
        "5. EXPERIMENT — \"а что если\", гипотеза с откатом/undo. Примеры: \"Давай попробуем на std::async, посмотрим скомпилится ли\"\n"
        "6. INFRA — обслуживание окружения без правки логики .h/.cpp. Примеры: \"Пересобери cmake\", \"Покажи граф таргетов\", \"Обнови compile_commands.json\". НЕ путать: \"покажи связи/зависимости метода/класса в коде\" = UNDERSTAND, а \"покажи граф таргетов / build_graph / cmake\" = INFRA.\n"
        "ПРАВИЛА РАЗРЕШЕНИЯ:\n"
        "- \"Отрефакторить\" + 1 метод = SANDBOX_FIX. \"Отрефакторить\" + фреймворк/много файлов/весь модуль = QUARRY_DESIGN. Если в фразе есть \"vs / или / против\" и вторая часть содержит широкий контекст (весь модуль, фреймворк) — выбирай QUARRY_DESIGN.\n"
        "- Вопрос + просьба править (\"объясни и исправь\") = выбирай FIX/DESIGN, а не UNDERSTAND.\n"
        "- Правка + \"покрой тестами\" без слов про эксперимент = FIX/DESIGN (SANDBOX_FIX если 1 метод), а не TEST_GEN.\n"
        "- Сомнение между FIX и EXPERIMENT -> EXPERIMENT. FIX vs QUARRY -> QUARRY.\n"
        "- Триггер EXPERIMENT: \"гипотеза\" + \"посмотрим\", \"а что если\", \"попробуем/посмотрим скомпилится ли\", \"откат/undo/redo\". Правка + тесты + \"посмотрим скомпилится ли / попробуй\" = EXPERIMENT, а не TEST_GEN.";

    ChatRequest req;
    req.system = systemPrompt;
    req.user = goalText;
    req.model = modelName_;
    req.temperature = temperature_;
    req.maxTokens = 400;
    req.wantJson = true;

    ChatResult res = api_->chat(req);
    if(!res.ok)
        return unavailable("модель недоступна (" + res.error + ")");
    std::string content = res.text;
    std::string finishReason = res.finishReason;
    if(content.empty()) {
        if(finishReason=="length")
            return unavailable("ответ модели обрезан лимитом токенов (finish_reason=length, content пуст)");
        return unavailable("ответ модели пуст");
    }
    json cls;
    try{ cls=json::parse(content); } catch(...){
        std::string ext=extractFirstJsonObject(content);
        if(ext.empty()) {
            if(finishReason=="length")
                return unavailable("ответ модели обрезан лимитом токенов (finish_reason=length): "+content.substr(0,120));
            return unavailable("модель вернула не-JSON: "+content.substr(0,120));
        }
        try{ cls=json::parse(ext); } catch(const std::exception& e){ return unavailable(std::string("не удалось распарсить JSON: ")+e.what()); }
    }
    // Модель иногда заворачивает ответ в лишний слой: {"result":{"primitive":"INFRA",...}}
    if(!cls.contains("primitive") && cls.size()==1){
        const auto& v = cls.begin().value();
        if(v.is_object() && v.contains("primitive")) cls = v;
    }
    std::string primStr = cls.value("primitive","");
    bool ok=false;
    Primitive p = primitiveFromString(primStr, ok);
    if(!ok) return unavailable("модель вернула недопустимый primitive: \""+primStr+"\"");
    PrimitiveGuess g{p, primStr, cls.value("rationale","(модель не пояснила)"), false};
    // Тип B: confidence + alternative. Принимаем и число, и строку ("0.9") —
    // модель на temp 0.7 регулярно отдаёт число строкой.
    if(cls.contains("confidence")){
        double c = 1.0; bool got = false;
        if(cls["confidence"].is_number()){ c = cls["confidence"].get<double>(); got = true; }
        else if(cls["confidence"].is_string()){
            try { c = std::stod(cls["confidence"].get<std::string>()); got = true; } catch(...) {}
        }
        if(got){
            if(c < 0.0) c = 0.0;
            if(c > 1.0) c = 1.0;
            g.confidence = c;
        }
    }
    if(cls.contains("alternative") && cls["alternative"].is_string()){
        std::string alt = cls["alternative"].get<std::string>();
        bool altOk=false;
        primitiveFromString(alt, altOk);
        if(altOk && alt != primStr) g.alternativePrimitiveStr = alt;
    }
    // goal_specific: отдельное от confidence измерение (см. промпт). Отсутствие поля
    // трактуем как true — не ломаем поведение, если модель его не прислала (старый ответ/temp).
    if(cls.contains("goal_specific") && cls["goal_specific"].is_boolean())
        g.goalIsSpecific = cls["goal_specific"].get<bool>();
    // Слой 1: detail. Нет поля -> GENERAL_QUESTION (безопасный дефолт).
    if(cls.contains("detail") && cls["detail"].is_string()){
        bool dOk=false;
        Detail d = detailFromString(cls["detail"].get<std::string>(), dOk);
        if(dOk){ g.detail = d; g.detailStr = detailToString(d); }
    }
    // Слой 0: cursor_binding. Нет поля -> NEEDED_PRESENT.
    if(cls.contains("cursor_binding") && cls["cursor_binding"].is_string()){
        std::string cb = cls["cursor_binding"].get<std::string>();
        if(cb == "NOT_NEEDED") g.cursorBinding = CursorBinding::NOT_NEEDED;
        else if(cb == "NEEDED_MISSING") g.cursorBinding = CursorBinding::NEEDED_MISSING;
        else if(cb == "NEEDED_PRESENT") g.cursorBinding = CursorBinding::NEEDED_PRESENT;
        g.cursorBindingStr = cb;
    }
    return g;
}

} // namespace cppagent
