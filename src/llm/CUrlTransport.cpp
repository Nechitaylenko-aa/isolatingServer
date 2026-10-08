/**
 * @file CUrlTransport.cpp
 * @brief Реализация HTTP-транспорта на libcurl (6b).
 */

#include "llm/CUrlTransport.h"

#include <cstddef>
#include <string>
#include <vector>

#include <curl/curl.h>

namespace fcpp
{

namespace
{

/**
 * @brief Callback приёма данных.
 * @param contents Данные.
 * @param size Размер элемента.
 * @param nmemb Число элементов.
 * @param userdata Указатель на SHttpResult.
 * @return Число принятых байт.
 */
std::size_t write_callback(char* contents, std::size_t size, std::size_t nmemb, void* userdata)
{
    auto* payload = static_cast<std::pair<SHttpResult*, const std::function<bool(const std::string&)>*>*>(userdata);
    const std::size_t total = size * nmemb;
    const std::string chunk(contents, total);

    payload->first->m_body.append(chunk);

    if (payload->second != nullptr && *payload->second)
    {
        if (!(*payload->second)(chunk))
        {
            return 0; // прервать передачу
        }
    }

    return total;
}

} // namespace

SHttpResult CUrlTransport::post_json(const std::string& url,
                                     const std::vector<std::string>& headers,
                                     const std::string& json_body,
                                     long timeout_seconds,
                                     const std::function<bool(const std::string&)>& on_chunk) const
{
    SHttpResult result;

    CURL* handle = curl_easy_init();
    if (handle == nullptr)
    {
        result.m_error = "curl init failed";
        return result;
    }

    curl_slist* header_list = nullptr;
    for (const std::string& header : headers)
    {
        header_list = curl_slist_append(header_list, header.c_str());
    }

    std::pair<SHttpResult*, const std::function<bool(const std::string&)>*> payload(&result, &on_chunk);

    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &payload);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

    const CURLcode code = curl_easy_perform(handle);
    if (code == CURLE_OK)
    {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &result.m_status);
        result.m_ok = (result.m_status >= 200 && result.m_status < 300);
        if (!result.m_ok)
        {
            result.m_error = "http status " + std::to_string(result.m_status);
        }
    }
    else
    {
        result.m_error = std::string("curl error: ") + curl_easy_strerror(code);
    }

    if (header_list != nullptr)
    {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(handle);

    return result;
}

} // namespace fcpp
