#include "model_broker.h"
#include <algorithm>
#include <cctype>
#include <curl/curl.h>
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
Primitive primitiveFromString(const std::string& s, bool& ok){
    if(s=="UNDERSTAND"){ok=true;return Primitive::UNDERSTAND;}
    if(s=="SANDBOX_FIX"){ok=true;return Primitive::SANDBOX_FIX;}
    if(s=="QUARRY_DESIGN"){ok=true;return Primitive::QUARRY_DESIGN;}
    if(s=="TEST_GEN"){ok=true;return Primitive::TEST_GEN;}
    if(s=="EXPERIMENT"){ok=true;return Primitive::EXPERIMENT;}
    if(s=="INFRA"){ok=true;return Primitive::INFRA;}
    ok=false;return Primitive::UNDERSTAND;
}

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}
bool containsAny(const std::string& text, std::initializer_list<const char*> needles) {
    for(auto* n: needles) if(text.find(n)!=std::string::npos) return true;
    return false;
}
PrimitiveGuess makeGuess(Primitive p, std::string rationale){
    return PrimitiveGuess{p, primitiveToString(p), std::move(rationale), false};
}
PrimitiveGuess unavailable(std::string reason){
    return PrimitiveGuess{Primitive::UNDERSTAND, "", std::move(reason), true};
}

} // anon

// Stub: детерминированная эвристика по 6 примитивам (заменится Light)
PrimitiveGuess StubModelBroker::classifyIntent(const std::string& goalText) {
    std::string t = toLower(goalText);
    // INFRA — сборка/окружение, без правки логики
    if(containsAny(t, {"пересобери","пересобрать","cmake","compile_commands","граф зависимост","build_graph","ninja","make "})) {
        // уточняем что это не "добавь cmake таргет" (это QUARRY)
        if(containsAny(t, {"покажи граф","пересобери","обнови compile","собери проект"}))
            return makeGuess(Primitive::INFRA, "команда обслуживания окружения/сборки без правки логики");
        if(t.find("инфра")!=std::string::npos)
            return makeGuess(Primitive::INFRA, "явное слово инфра");
    }
    // EXPERIMENT — гипотеза с откатом
    if(containsAny(t, {"попробу","а что если","что если","эксперимент","гипотеза","посмотрим","скомпилируется ли","откат","undo","redo"}))
        return makeGuess(Primitive::EXPERIMENT, "формулировка гипотезы/эксперимента с возможным откатом");
    // TEST_GEN — цель == тесты
    if(containsAny(t, {"напиши тест","покрой тест","покрыть тест","юнит-тест","unit test","google test","gtest","тест для"})){
        // "попробуй переписать и покрой тестами" -> не TEST_GEN
        if(containsAny(t, {"перепиши","исправь","замени","отрефактор","добавь класс"}))
            ; // fall through to FIX/DESIGN
        else
            return makeGuess(Primitive::TEST_GEN, "явная просьба написать/покрыть тестами");
    }
    // UNDERSTAND — вопрос без просьбы менять код
    bool isQuestion = containsAny(t, {"что делает","объясни","где ","найди где","как работает","почему","утечка","логическ","покажи связи","clang_visit"});
    bool wantsChange = containsAny(t, {"исправь","почини","замени","отрефактор","добавь","создай","вынеси","перепиши","оптимизир"});
    if(isQuestion && !wantsChange)
        return makeGuess(Primitive::UNDERSTAND, "вопрос/объяснение без просьбы менять код на диске");
    // QUARRY_DESIGN — новые файлы/модули/архитектура
    if(containsAny(t, {"новый класс","новый модуль","новый файл","добавь брокер","скелет плагина","вынеси логику","отдельный сервис","архитектур","воркфлоу","workflow","нескольк компонент","много класс"}))
        return makeGuess(Primitive::QUARRY_DESIGN, "просьба создать новые модули/файлы или изменить архитектуру");
    // SANDBOX_FIX — точечная правка под курсором
    if(containsAny(t, {"этот метод","эту функцию","в этом методе","в этом классе","исправь баг","выход за границ","сырой указатель","unique_ptr","исправь"}))
        return makeGuess(Primitive::SANDBOX_FIX, "точечная правка одного метода/класса под курсором");
    // Правила разрешения конфликтов из ТЗ
    if(wantsChange) {
        // широкое vs точечное
        if(containsAny(t, {"весь класс","весь модуль","фреймворк","сервис"}))
            return makeGuess(Primitive::QUARRY_DESIGN, "конфликт отрефакторить+широкий контекст -> QUARRY_DESIGN");
        return makeGuess(Primitive::SANDBOX_FIX, "конфликт по умолчанию для правки одного места -> SANDBOX_FIX");
    }
    // дефолт: не распознали — считаем вопросом, безопасно
    return makeGuess(Primitive::UNDERSTAND, "явных сигналов правки нет — трактуем как UNDERSTAND (read-only безопасно)");
}

// --- LightModelBroker: HTTP к llama.cpp ---

namespace {

size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata){
    auto* buf=static_cast<std::string*>(userdata);
    buf->append(ptr, size*nmemb);
    return size*nmemb;
}
std::string extractFirstJsonObject(const std::string& text){
    auto start=text.find('{');
    if(start==std::string::npos) return "";
    int depth=0;
    for(size_t i=start;i<text.size();++i){
        if(text[i]=='{') ++depth;
        else if(text[i]=='}'){ --depth; if(depth==0) return text.substr(start,i-start+1); }
    }
    return "";
}
struct HttpResult{ bool ok=false; long httpCode=0; std::string body; std::string curlError; };
HttpResult postChatCompletion(const std::string& url, const json& req){
    HttpResult r;
    CURL* curl=curl_easy_init();
    if(!curl){ r.curlError="curl_easy_init вернул nullptr"; return r; }
    std::string bodyStr=req.dump();
    std::string resp;
    struct curl_slist* headers=nullptr;
    headers=curl_slist_append(headers,"Content-Type: application/json");
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_POST,1L);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,bodyStr.c_str());
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,curlWriteCallback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&resp);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,30L);
    CURLcode rc=curl_easy_perform(curl);
    long code=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
    curl_slist_free_all(headers); curl_easy_cleanup(curl);
    if(rc!=CURLE_OK){ r.curlError=curl_easy_strerror(rc); return r; }
    r.httpCode=code; r.body=resp; r.ok=(code>=200 && code<300);
    return r;
}
} // anon

PrimitiveGuess LightModelBroker::classifyIntent(const std::string& goalText){
    const std::string systemPrompt =
        "Ты — детерминированный системный шлюз C++ агента. Твоя ЕДИНСТВЕННАЯ задача — перевести техническое задание пользователя в один из 6 системных примитивов.\n"
        "ОТВЕЧАЙ СТРОГО ВАЛИДНЫМ JSON БЕЗ MARKDOWN: {\"primitive\": \"UNDERSTAND\"|\"SANDBOX_FIX\"|\"QUARRY_DESIGN\"|\"TEST_GEN\"|\"EXPERIMENT\"|\"INFRA\", \"confidence\": 0.0..1.0, \"alternative\": \"второй по вероятности примитив или пустая строка\", \"rationale\": \"одна фраза по-русски\"}\n"
        "confidence — НАСКОЛЬКО ТЫ УВЕРЕН в выбранном primitive (0.0 = совсем не уверен, 1.0 = абсолютно уверен). Будь ЧЕСТЕН: на неоднозначных фразах ставь 0.4-0.6 и указывай alternative, не завышай.\n"
        "КРИТЕРИИ:\n"
        "1. UNDERSTAND — вопрос/объяснение/поиск. Код на диске НЕ меняется. Примеры: \"Что делает этот метод?\", \"Где утечка?\", \"Объясни clang_visitChildren\"\n"
        "2. SANDBOX_FIX — починить/отрефакторить ОДИН метод/класс под курсором. Локально. Примеры: \"Замени на unique_ptr в этом методе\", \"Исправь выход за границы\"\n"
        "3. QUARRY_DESIGN — новая сущность/файлы/архитектура/воркфлоу >1 компонента. Требует ветки. Примеры: \"Добавь брокер для OpenAI\", \"Вынеси AST в сервис\"\n"
        "4. TEST_GEN — цель == тесты. Примеры: \"Напиши тесты для compile_commands.cpp на gtest\"\n"
        "5. EXPERIMENT — \"а что если\", гипотеза с откатом/undo. Примеры: \"Давай попробуем на std::async, посмотрим скомпилится ли\"\n"
        "6. INFRA — обслуживание окружения без правки логики .h/.cpp. Примеры: \"Пересобери cmake\", \"Покажи граф таргетов\", \"Обнови compile_commands.json\". НЕ путать: \"покажи связи/зависимости метода/класса в коде\" = UNDERSTAND, а \"покажи граф таргетов / build_graph / cmake\" = INFRA.\n"
        "ПРАВИЛА РАЗРЕШЕНИЯ:\n"
        "- \"Отрефакторить\" + 1 метод = SANDBOX_FIX. \"Отрефакторить\" + фреймворк/много файлов/весь модуль = QUARRY_DESIGN. Если в фразе есть \"vs / или / против\" и вторая часть содержит широкий контекст (весь модуль, фреймворк) — выбирай QUARRY_DESIGN.\n"
        "- Вопрос + просьба править (\"объясни и исправь\") = выбирай FIX/DESIGN, а не UNDERSTAND.\n"
        "- Правка + \"покрой тестами\" без слов про эксперимент = FIX/DESIGN (SANDBOX_FIX если 1 метод), а не TEST_GEN.\n"
        "- Сомнение между FIX и EXPERIMENT -> EXPERIMENT. FIX vs QUARRY -> QUARRY.\n"
        "- Триггер EXPERIMENT: \"гипотеза\" + \"посмотрим\", \"а что если\", \"попробуем/посмотрим скомпилится ли\", \"откат/undo/redo\". Правка + тесты + \"посмотрим скомпилится ли / попробуй\" = EXPERIMENT, а не TEST_GEN.";

    json requestBody = {
        {"messages", json::array({
            {{"role","system"},{"content", systemPrompt}},
            {{"role","user"},{"content", goalText}}
        })},
        {"temperature", temperature_},
        {"max_tokens", 250}
    };
    if(!modelName_.empty()) requestBody["model"]=modelName_;
    std::string url = baseUrl_ + "/v1/chat/completions";
    json withFormat = requestBody;
    withFormat["response_format"] = {{"type","json_object"}};

    HttpResult res = postChatCompletion(url, withFormat);
    if(!res.ok){
        HttpResult retry = postChatCompletion(url, requestBody);
        if(retry.ok) res=retry;
        else {
            std::string reason = !res.curlError.empty() ? ("сеть: "+res.curlError) : ("HTTP "+std::to_string(res.httpCode)+": "+res.body);
            return unavailable("модель недоступна ("+reason+")");
        }
    }
    json responseJson;
    try{ responseJson=json::parse(res.body); } catch(const std::exception& e){ return unavailable(std::string("ответ сервера не JSON: ")+e.what()); }
    if(!responseJson.contains("choices") || responseJson["choices"].empty())
        return unavailable("в ответе нет choices[] — не похоже на OpenAI-совместимый формат");
    std::string content = responseJson["choices"][0]["message"].value("content","");
    if(content.empty()) return unavailable("choices[0].message.content пуст");
    json cls;
    try{ cls=json::parse(content); } catch(...){
        std::string ext=extractFirstJsonObject(content);
        if(ext.empty()) return unavailable("модель вернула не-JSON: "+content.substr(0,120));
        try{ cls=json::parse(ext); } catch(const std::exception& e){ return unavailable(std::string("не удалось распарсить JSON: ")+e.what()); }
    }
    std::string primStr = cls.value("primitive","");
    bool ok=false;
    Primitive p = primitiveFromString(primStr, ok);
    if(!ok) return unavailable("модель вернула недопустимый primitive: \""+primStr+"\"");
    PrimitiveGuess g{p, primStr, cls.value("rationale","(модель не пояснила)"), false};
    // Тип B: confidence + alternative (если модель прислала)
    if(cls.contains("confidence") && cls["confidence"].is_number()){
        g.confidence = cls["confidence"].get<double>();
        if(g.confidence < 0.0) g.confidence = 0.0;
        if(g.confidence > 1.0) g.confidence = 1.0;
    }
    if(cls.contains("alternative") && cls["alternative"].is_string()){
        std::string alt = cls["alternative"].get<std::string>();
        bool altOk=false;
        primitiveFromString(alt, altOk);
        if(altOk && alt != primStr) g.alternativePrimitiveStr = alt;
    }
    return g;
}

} // namespace cppagent
