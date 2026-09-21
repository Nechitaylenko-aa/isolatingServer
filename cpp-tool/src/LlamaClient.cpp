#include "LlamaClient.h"
#include "JsonUtils.h"
#include <curl/curl.h>
#include <iostream>

size_t LlamaClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}

std::string LlamaClient::query(const std::string& prompt) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) return "Error: curl init failed";

    std::string escaped = JsonUtils::escape(prompt);
    std::string json = "{\"prompt\": \"" + escaped + "\", \"n_predict\": 4000, \"temperature\": 0.2, \"stop\": []}";
    // Вместо "stop": []
    //std::string json = "{\"prompt\": \"" + escaped + "\", \"n_predict\": 1000, \"temperature\": 0.2, \"stop\": [\"## \", \", \"---\"]}";

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8080/completion");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Error: " + std::string(curl_easy_strerror(res));
    }

    // Парсим JSON
    size_t pos = response.find("\"content\":\"");
    if (pos == std::string::npos) {
        return "Error: cannot parse JSON response";
    }

    pos += 11;
    std::string result;
    bool escape = false;
    for (size_t i = pos; i < response.length(); i++) {
        char c = response[i];
        if (escape) {
            if (c == 'n') result += '\n';
            else if (c == 'r') result += '\r';
            else if (c == 't') result += '\t';
            else if (c == '\\') result += '\\';
            else if (c == '"') result += '"';
            else result += c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            break;
        } else {
            result += c;
        }
    }

    return result;
}
