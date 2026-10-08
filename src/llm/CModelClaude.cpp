/**
 * @file CModelClaude.cpp
 * @brief Реализация провайдера Claude (6b).
 */

#include "llm/CModelClaude.h"

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

/** @brief Путь messages (добавляется к base_url до /v1). */
constexpr const char* k_messages_path = "/messages";

/** @brief Версия Anthropic API. */
constexpr const char* k_anthropic_version = "2023-06-01";

/** @brief Таймаут запроса (сек). */
constexpr long k_timeout_seconds = 300;

/**
 * @brief Разобрать потоковую строку content_block_delta.
 * @param line Строка SSE.
 * @return Дельта либо пустая строка.
 */
std::string parse_sse_delta(const std::string& line)
{
    const std::string prefix = "data: ";
    if (line.rfind(prefix, 0) != 0)
    {
        return {};
    }

    try
    {
        const nlohmann::json document = nlohmann::json::parse(line.substr(prefix.size()));
        if (document.contains("type") && document["type"] == "content_block_delta"
                && document.contains("delta") && document["delta"].contains("text")
                && document["delta"]["text"].is_string())
        {
            return document["delta"]["text"].get<std::string>();
        }
    }
    catch (const nlohmann::json::exception&)
    {
        return {};
    }
    return {};
}

} // namespace

CModelClaude::CModelClaude(SModelEndpoint endpoint, double temperature, int max_tokens)
    : m_endpoint(std::move(endpoint))
    , m_temperature(temperature)
    , m_max_tokens(max_tokens)
{
}

bool CModelClaude::is_available() const
{
    return !m_endpoint.m_base_url.empty() && !m_endpoint.m_model.empty();
}

std::vector<std::string> CModelClaude::build_headers() const
{
    std::vector<std::string> headers =
    {
        "Content-Type: application/json",
        std::string("anthropic-version: ") + k_anthropic_version
    };
    if (!m_endpoint.m_api_key.empty())
    {
        headers.push_back("x-api-key: " + m_endpoint.m_api_key);
    }
    return headers;
}

std::string CModelClaude::build_body(const std::vector<SModelMessage>& messages, bool stream) const
{
    nlohmann::json document;
    document["model"] = m_endpoint.m_model;
    document["max_tokens"] = m_max_tokens;
    document["temperature"] = m_temperature;
    document["stream"] = stream;

    std::string system_text;
    nlohmann::json array = nlohmann::json::array();
    for (const SModelMessage& message : messages)
    {
        if (message.m_role == EModelRole::System)
        {
            if (!system_text.empty())
            {
                system_text += "\n";
            }
            system_text += message.m_content;
            continue;
        }
        const char* role = (message.m_role == EModelRole::Assistant) ? "assistant" : "user";
        array.push_back({{"role", role}, {"content", message.m_content}});
    }
    if (!system_text.empty())
    {
        document["system"] = system_text;
    }
    document["messages"] = array;

    return document.dump();
}

SModelReply CModelClaude::complete(const std::vector<SModelMessage>& messages)
{
    SModelReply reply;
    if (!is_available())
    {
        reply.m_error = "model endpoint not configured";
        return reply;
    }

    const CUrlTransport transport;
    const SHttpResult http = transport.post_json(
        CEndpointUtil::join(m_endpoint.m_base_url, k_messages_path),
        build_headers(), build_body(messages, false), k_timeout_seconds, nullptr);

    if (!http.m_ok)
    {
        reply.m_error = http.m_error;
        return reply;
    }

    try
    {
        const nlohmann::json document = nlohmann::json::parse(http.m_body);
        if (document.contains("content") && document["content"].is_array() && !document["content"].empty()
                && document["content"][0].contains("text"))
        {
            reply.m_text = document["content"][0]["text"].get<std::string>();
        }
    }
    catch (const nlohmann::json::exception&)
    {
        reply.m_text.clear();
    }

    reply.m_ok = !reply.m_text.empty();
    if (!reply.m_ok)
    {
        reply.m_error = "empty model response";
    }
    return reply;
}

SModelReply CModelClaude::complete_stream(const std::vector<SModelMessage>& messages,
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

    const CUrlTransport transport;
    const SHttpResult http = transport.post_json(
        CEndpointUtil::join(m_endpoint.m_base_url, k_messages_path),
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

                const std::string delta = parse_sse_delta(line);
                if (!delta.empty())
                {
                    accumulated += delta;
                    if (on_chunk && !on_chunk(delta))
                    {
                        return false;
                    }
                }
            }
            return true;
        });

    if (!http.m_ok)
    {
        reply.m_error = http.m_error;
        return reply;
    }

    reply.m_text = accumulated;
    reply.m_ok = !accumulated.empty();
    if (!reply.m_ok)
    {
        reply.m_error = "empty streamed response";
    }
    return reply;
}

} // namespace fcpp
