#include "main_model_broker.h"
#include <curl/curl.h>
#include "json.hpp"
#include <algorithm>

namespace cppagent {

namespace {
size_t curlWriteCb(char* ptr, size_t size, size_t nmemb, void* d){ auto* s=static_cast<std::string*>(d); s->append(ptr,size*nmemb); return size*nmemb; }
struct HttpRes{ bool ok=false; long code=0; std::string body; std::string err; };
HttpRes postJson(const std::string& url, const json& bodyJson){
    HttpRes r; CURL* c=curl_easy_init(); if(!c){ r.err="curl_easy_init null"; return r; }
    std::string bs=bodyJson.dump(); std::string resp;
    struct curl_slist* h=nullptr; h=curl_slist_append(h,"Content-Type: application/json");
    curl_easy_setopt(c,CURLOPT_URL,url.c_str()); curl_easy_setopt(c,CURLOPT_POST,1L); curl_easy_setopt(c,CURLOPT_POSTFIELDS,bs.c_str());
    curl_easy_setopt(c,CURLOPT_HTTPHEADER,h); curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,curlWriteCb); curl_easy_setopt(c,CURLOPT_WRITEDATA,&resp); curl_easy_setopt(c,CURLOPT_TIMEOUT,120L);
    CURLcode rc=curl_easy_perform(c); long code=0; curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);
    curl_slist_free_all(h); curl_easy_cleanup(c);
    if(rc!=CURLE_OK){ r.err=curl_easy_strerror(rc); return r; }
    r.code=code; r.body=resp; r.ok=(code>=200 && code<300); return r;
}
} // anon

static std::string truncateUtf8(const std::string& s, size_t maxBytes){
    if(s.size()<=maxBytes) return s;
    size_t n=maxBytes;
    while(n>0 && (static_cast<unsigned char>(s[n]) & 0xC0)==0x80) --n;
    // не режем в середине символа — откатываемся к началу символа
    // если попали на ведущий байт 2-4 байтного символа, отступим ещё
    if(n>0 && (static_cast<unsigned char>(s[n]) & 0xC0)==0xC0) {
        // оставляем до n, сам ведущий байт уже не влезает целиком
    }
    // Проверяем что символ целиком влезал: если s[n] — ведущий байт, а мы его отрезали на границе,
    // то n сейчас на ведущем байте, а его continuation байты за границей — отбрасываем его тоже
    if(n<maxBytes && n < s.size() && (static_cast<unsigned char>(s[n]) & 0x80)){
        // s[n] — начало многобайтного символа, который не влезает — отбрасываем
        // оставляем до n
    } else if(n==maxBytes){
        // ровно на continuation — уже откатились
    }
    // Уточнение: просто ищем последний байт не-continuation и проверяем длину символа
    // Проще: идём назад пока continuation, потом проверяем влезает ли символ целиком
    size_t cut=n;
    // если n указывает на ведущий байт, проверим влезает ли весь символ
    if(cut < s.size() && (static_cast<unsigned char>(s[cut]) & 0x80)){
        unsigned char lead=static_cast<unsigned char>(s[cut]);
        size_t need=1;
        if((lead & 0xE0)==0xC0) need=2;
        else if((lead & 0xF0)==0xE0) need=3;
        else if((lead & 0xF8)==0xF0) need=4;
        if(cut+need > maxBytes) {
            // не влезает — режем до cut
        } else {
            cut = cut + need;
        }
    }
    return s.substr(0, cut);
}
GenerateResult StubMainModelBroker::generate(const GenerateRequest& req){
    GenerateResult r; r.ok=true;
    r.planText = "[STUB-MAIN] Primitive=" + req.primitiveStr + ", goal=" + truncateUtf8(req.goalText,120) + ", method=" + req.methodSignature.substr(0,120) + ", scale=" + req.scaleFacts.dump();
    r.rationale = "stub-main S4.1 — детерминированный plan+actions, main 12GiB приедет 29.09";
    if(req.primitiveStr=="SANDBOX_FIX" || req.primitiveStr=="QUARRY_DESIGN" || req.primitiveStr=="EXPERIMENT" || req.primitiveStr=="TEST_GEN"){
        if(!req.file.empty()){
            std::string g = truncateUtf8(req.goalText, 80);
            std::string insert = "\n// stub-main " + req.primitiveStr + ": " + g + "\n";
            r.actions = json::array({ json{{"oldText", ""}, {"newText", insert}} });
        }
    } else {
        r.actions = json::array();
    }
    return r;
}

GenerateResult HttpMainModelBroker::generate(const GenerateRequest& req){
    std::string systemPrompt =
        "Ты — C++ ассистент-генератор правок. По примитиву и контексту метода сгенерируй план и точные правки файлов.\n"
        "Отвечай СТРОГО JSON без markdown: {\"plan\": \"...шаги...\", \"rationale\": \"...\", \"actions\": [{\"oldText\": \"точный кусок для замены или \\\"\" для вставки в конец\", \"newText\": \"...\"}] }\n"
        "НЕ указывай поле \"file\" — файл уже известен системе (берётся из курсора), ты генерируешь только oldText/newText.\n"
        "Правила: oldText должен точно совпадать с содержимым файла (иначе замена провалится), для вставки ставь oldText=\"\".";
    std::string userPrompt = "primitive=" + req.primitiveStr + "\ngoal=" + req.goalText + "\nfile=" + req.file + "\nmethod=" + req.methodSignature + "\nusr=" + req.methodUSR + "\nscale=" + req.scaleFacts.dump();
    // Без фрагмента файла модель не может выдать oldText, точно совпадающий с содержимым:
    // она его не видела и либо выдумывает несуществующую строку, либо вставляет в конец (oldText:"").
    if(!req.fileContext.empty()) userPrompt += "\n--- file content (with line numbers) ---\n" + req.fileContext;
    json body = {{"messages", json::array({{{"role","system"},{"content",systemPrompt}}, {{"role","user"},{"content",userPrompt}}})}, {"temperature",0.2}, {"max_tokens",1200}};
    if(!modelName_.empty()) body["model"]=modelName_;
    std::string url = baseUrl_ + "/v1/chat/completions";
    json withFormat=body; withFormat["response_format"]={{"type","json_object"}};
    HttpRes res = postJson(url, withFormat);
    if(!res.ok){ HttpRes retry=postJson(url, body); if(retry.ok) res=retry; else { GenerateResult gr; gr.ok=false; gr.modelUnavailable=true; gr.error=!res.err.empty()?("сеть: "+res.err):("HTTP "+std::to_string(res.code)+": "+res.body); return gr; } }
    json j; try{ j=json::parse(res.body);}catch(const std::exception& e){ GenerateResult gr; gr.ok=false; gr.modelUnavailable=true; gr.error=std::string("ответ не JSON: ")+e.what(); return gr; }
    if(!j.contains("choices")||j["choices"].empty()){ GenerateResult gr; gr.ok=false; gr.modelUnavailable=true; gr.error="нет choices[]"; return gr; }
    std::string content=j["choices"][0]["message"].value("content","");
    if(content.empty()){ GenerateResult gr; gr.ok=false; gr.modelUnavailable=true; gr.error="content пуст"; return gr; }
    json inner; try{ inner=json::parse(content);}catch(...){ // попробуем вырезать {...}
        auto s=content.find('{'); auto e=content.rfind('}'); if(s!=std::string::npos && e!=std::string::npos && e>s){ try{ inner=json::parse(content.substr(s,e-s+1)); }catch(...){ GenerateResult gr; gr.ok=true; gr.planText=content; gr.rationale="модель вернула текст без JSON"; return gr; } } else { GenerateResult gr; gr.ok=true; gr.planText=content; gr.rationale="текст"; return gr; }
    }
    GenerateResult gr; gr.ok=true; gr.planText=inner.value("plan", content); gr.rationale=inner.value("rationale", "");
    if(inner.contains("actions") && inner["actions"].is_array()) gr.actions = inner["actions"];
    else gr.actions = json::array();
    // валидация actions: путь приходит из Ground, а не от модели — проверяем только
    // наличие содержимого правки (oldText/newText), поле "file" не требуется.
    json filtered=json::array();
    for(auto& a: gr.actions) if(a.is_object() && (a.contains("oldText") || a.contains("newText"))) filtered.push_back(a);
    gr.actions = filtered;
    return gr;
}

} // namespace cppagent
