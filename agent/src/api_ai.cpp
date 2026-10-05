#include "api_ai.h"

#include "json.hpp"
#include <curl/curl.h>

namespace cppagent {

using json = nlohmann::json;

namespace {

size_t curlWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

struct HttpResult { bool ok{false}; long httpCode{0}; std::string body; std::string curlError; };

HttpResult postJson(const std::string& url, const json& bodyJson,
                    const std::string& apiKey, long connectTimeoutSec, long timeoutSec) {
    HttpResult r;
    // libcurl без явного init работает не везде (и ломается при параллельных вызовах из тестов).
    static const bool curlInit = []() { curl_global_init(CURL_GLOBAL_DEFAULT); return true; }();
    (void)curlInit;

    CURL* curl = curl_easy_init();
    if (!curl) { r.curlError = "curl_easy_init вернул nullptr"; return r; }
    std::string bodyStr = bodyJson.dump();
    std::string resp;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!apiKey.empty())
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    // Локальные модели: короткий connect-таймаут отсекает мёртвый сервер,
    // длинную генерацию не режем.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) { r.curlError = curl_easy_strerror(rc); return r; }
    r.httpCode = code; r.body = resp; r.ok = (code >= 200 && code < 300);
    return r;
}

} // namespace

std::string extractFirstJsonObject(const std::string& text) {
    auto start = text.find('{');
    if (start == std::string::npos) return "";
    int depth = 0;
    bool inStr = false, esc = false;
    for (size_t i = start; i < text.size(); ++i) {
        char c = text[i];
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') { inStr = true; continue; }
        if (c == '{') ++depth;
        else if (c == '}') { --depth; if (depth == 0) return text.substr(start, i - start + 1); }
    }
    return "";
}

CApiOpenAI::CApiOpenAI(std::string baseUrl, std::string modelName,
                       std::string apiKey, long connectTimeoutSec, long timeoutSec)
    : baseUrl_(std::move(baseUrl)), modelName_(std::move(modelName)),
      apiKey_(std::move(apiKey)),
      connectTimeoutSec_(connectTimeoutSec), timeoutSec_(timeoutSec) {}

ChatResult CApiOpenAI::chat(const ChatRequest& req) {
    ChatResult out;
    json requestBody = {
        {"messages", json::array({
            {{"role","system"},{"content", req.system}},
            {{"role","user"},{"content", req.user}}
        })},
        {"temperature", req.temperature},
        {"max_tokens", req.maxTokens}
    };
    std::string model = req.model.empty() ? modelName_ : req.model;
    if (!model.empty()) requestBody["model"] = model;

    std::string url = baseUrl_ + "/v1/chat/completions";
    json withFormat = requestBody;
    if (req.wantJson) withFormat["response_format"] = {{"type","json_object"}};

    HttpResult res = postJson(url, withFormat, apiKey_, connectTimeoutSec_, timeoutSec_);
    if (req.wantJson && !res.ok) {
        // Сервер может не поддерживать response_format — повторяем без него.
        HttpResult retry = postJson(url, requestBody, apiKey_, connectTimeoutSec_, timeoutSec_);
        if (retry.ok) res = retry;
    }
    if (!res.ok) {
        out.error = !res.curlError.empty() ? ("сеть: " + res.curlError)
                                           : ("HTTP " + std::to_string(res.httpCode) + ": " + res.body);
        return out;
    }

    json responseJson;
    try { responseJson = json::parse(res.body); }
    catch (const std::exception& e) { out.error = std::string("ответ сервера не JSON: ") + e.what(); return out; }

    if (!responseJson.contains("choices") || responseJson["choices"].empty()) {
        out.error = "в ответе нет choices[] — не похоже на OpenAI-совместимый формат";
        return out;
    }
    // Текст может прийти в content; некоторые диалекты кладут объект.
    auto& choice = responseJson["choices"][0];
    if (choice.contains("message") && choice["message"].contains("content") && choice["message"]["content"].is_string())
        out.text = choice["message"].value("content", "");
    else if (choice.contains("text"))
        out.text = choice.value("text", "");
    out.finishReason = choice.value("finish_reason", "");
    out.ok = true;
    return out;
}

} // namespace cppagent
