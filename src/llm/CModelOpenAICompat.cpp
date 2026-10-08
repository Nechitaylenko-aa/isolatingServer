/**
 * @file CModelOpenAICompat.cpp
 * @brief Реализация OpenAI-совместимого провайдера (6b).
 */

#include "llm/CModelOpenAICompat.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "llm/CEndpointUtil.h"
#include "llm/CUrlTransport.h"

namespace fcpp
{

namespace
{

/** @brief Путь chat completions (добавляется к base_url до /v1). */
constexpr const char* k_chat_path = "/chat/completions";

/** @brief Таймаут запроса (сек). */
constexpr long k_timeout_seconds = 300;

/**
 * @brief Сопоставить роль с именем роли OpenAI.
 * @param role Роль.
 * @return Имя.
 */
const char* role_to_string(EModelRole role)
{
    if (role == EModelRole::System)
    {
        return "system";
    }
    if (role == EModelRole::Assistant)
    {
        return "assistant";
    }
    return "user";
}

/**
 * @brief Разобрать одну SSE-строку и вернуть delta-текст.
 * @param line Строка.
 * @return Дельта либо пустая строка.
 */
std::string parse_sse_line(const std::string& line)
{
    const std::string prefix = "data: ";
    if (line.rfind(prefix, 0) != 0)
    {
        return {};
    }

    const std::string payload = line.substr(prefix.size());
    if (payload == "[DONE]")
    {
        return {};
    }

    try
    {
        const nlohmann::json document = nlohmann::json::parse(payload);
        if (document.contains("choices") && document["choices"].is_array() && !document["choices"].empty())
        {
            const nlohmann::json& choice = document["choices"][0];
            if (choice.contains("delta") && choice["delta"].contains("content")
                    && choice["delta"]["content"].is_string())
            {
                return choice["delta"]["content"].get<std::string>();
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        return {};
    }
    return {};
}

} // namespace

CModelOpenAICompat::CModelOpenAICompat(SModelEndpoint endpoint, double temperature, int max_tokens)
    : m_endpoint(std::move(endpoint))
    , m_temperature(temperature)
    , m_max_tokens(max_tokens)
{
}

bool CModelOpenAICompat::is_available() const
{
    return !m_endpoint.m_base_url.empty() && !m_endpoint.m_model.empty();
}

std::vector<std::string> CModelOpenAICompat::build_headers() const
{
    std::vector<std::string> headers = {"Content-Type: application/json"};
    if (!m_endpoint.m_api_key.empty())
    {
        headers.push_back("Authorization: Bearer " + m_endpoint.m_api_key);
    }
    return headers;
}

std::string CModelOpenAICompat::build_body(const std::vector<SModelMessage>& messages, bool stream) const
{
    nlohmann::json document;
    document["model"] = m_endpoint.m_model;
    document["temperature"] = m_temperature;
    document["max_tokens"] = m_max_tokens;
    document["stream"] = stream;

    nlohmann::json array = nlohmann::json::array();
    for (const SModelMessage& message : messages)
    {
        array.push_back({{"role", role_to_string(message.m_role)}, {"content", message.m_content}});
    }
    document["messages"] = array;

    return document.dump();
}

std::string CModelOpenAICompat::extract_content(const std::string& body)
{
    try
    {
        const nlohmann::json document = nlohmann::json::parse(body);
        if (document.contains("choices") && document["choices"].is_array() && !document["choices"].empty())
        {
            const nlohmann::json& choice = document["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")
                    && choice["message"]["content"].is_string())
            {
                return choice["message"]["content"].get<std::string>();
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        return {};
    }
    return {};
}

SModelReply CModelOpenAICompat::complete(const std::vector<SModelMessage>& messages)
{
    SModelReply reply;
    if (!is_available())
    {
        reply.m_error = "model endpoint not configured";
        return reply;
    }

    const CUrlTransport transport;
    const SHttpResult http = transport.post_json(
        CEndpointUtil::join(m_endpoint.m_base_url, k_chat_path),
        build_headers(), build_body(messages, false), k_timeout_seconds, nullptr);

    if (!http.m_ok)
    {
        reply.m_error = http.m_error;
        return reply;
    }

    reply.m_text = extract_content(http.m_body);
    reply.m_ok = !reply.m_text.empty();
    if (!reply.m_ok)
    {
        reply.m_error = "empty model response";
    }
    return reply;
}

SModelReply CModelOpenAICompat::complete_stream(const std::vector<SModelMessage>& messages,
                                                const SStreamCallback& on_chunk)
{
    SModelReply reply;
    if (!is_available())
    {
        reply.m_error = "model endpoint not configured";
        return reply;
    }

    std::string line_buffer;
    std::string accumulated;
    bool aborted = false;

    const CUrlTransport transport;
    const SHttpResult http = transport.post_json(
        CEndpointUtil::join(m_endpoint.m_base_url, k_chat_path),
        build_headers(), build_body(messages, true), k_timeout_seconds,
        [&](const std::string& chunk) -> bool
        {
            line_buffer.append(chunk);
            std::size_t newline = 0;
            while ((newline = line_buffer.find('\n')) != std::string::npos)
            {
                std::string line = line_buffer.substr(0, newline);
                line_buffer.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }

                const std::string delta = parse_sse_line(line);
                if (!delta.empty())
                {
                    accumulated += delta;
                    if (on_chunk && !on_chunk(delta))
                    {
                        aborted = true;
                        return false;
                    }
                }
            }
            return true;
        });

    if (!http.m_ok && !aborted)
    {
        reply.m_error = http.m_error;
        return reply;
    }

    reply.m_text = accumulated;
    reply.m_ok = !accumulated.empty();
    if (!reply.m_ok && !aborted)
    {
        reply.m_error = "empty streamed response";
    }
    return reply;
}

} // namespace fcpp
