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
    if (role == EModelRole::Tool)
    {
        return "tool";
    }
    return "user";
}

/**
 * @brief Разобрать tool_calls из сообщения ответа.
 * @param message JSON-объект message.
 * @return Список вызовов.
 */
std::vector<SModelToolCall> parse_tool_calls(const nlohmann::json& message)
{
    std::vector<SModelToolCall> calls;
    if (!message.contains("tool_calls") || !message["tool_calls"].is_array())
    {
        return calls;
    }
    for (const nlohmann::json& entry : message["tool_calls"])
    {
        if (!entry.contains("function"))
        {
            continue;
        }
        const nlohmann::json& function = entry["function"];
        SModelToolCall call;
        if (entry.contains("id") && entry["id"].is_string())
        {
            call.m_id = entry["id"].get<std::string>();
        }
        if (function.contains("name") && function["name"].is_string())
        {
            call.m_name = function["name"].get<std::string>();
        }
        if (function.contains("arguments"))
        {
            if (function["arguments"].is_string())
            {
                call.m_arguments = function["arguments"].get<std::string>();
            }
            else
            {
                call.m_arguments = function["arguments"].dump();
            }
        }
        calls.push_back(std::move(call));
    }
    return calls;
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
    // base_url обязателен; имя модели опционально (локальные серверы, напр.
    // llama.cpp, подставляют загруженную модель при пустом "model").
    return !m_endpoint.m_base_url.empty();
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

std::string CModelOpenAICompat::build_body(const std::vector<SModelMessage>& messages,
                                           bool stream,
                                           const std::vector<SModelTool>& tools) const
{
    nlohmann::json document;
    document["model"] = m_endpoint.m_model;
    document["temperature"] = m_temperature;
    document["max_tokens"] = m_max_tokens;
    document["stream"] = stream;

    nlohmann::json array = nlohmann::json::array();
    for (const SModelMessage& message : messages)
    {
        nlohmann::json entry;
        entry["role"] = role_to_string(message.m_role);
        entry["content"] = message.m_content;

        if (message.m_role == EModelRole::Tool && !message.m_tool_call_id.empty())
        {
            entry["tool_call_id"] = message.m_tool_call_id;
        }

        if (message.m_role == EModelRole::Assistant && !message.m_tool_calls.empty())
        {
            nlohmann::json calls = nlohmann::json::array();
            for (const SModelToolCall& call : message.m_tool_calls)
            {
                calls.push_back({{"id", call.m_id},
                                 {"type", "function"},
                                 {"function", {{"name", call.m_name},
                                               {"arguments", call.m_arguments}}}});
            }
            entry["tool_calls"] = calls;
        }

        array.push_back(std::move(entry));
    }
    document["messages"] = array;

    if (!tools.empty())
    {
        nlohmann::json tool_array = nlohmann::json::array();
        for (const SModelTool& tool : tools)
        {
            nlohmann::json properties = nlohmann::json::object();
            nlohmann::json required = nlohmann::json::array();
            for (const SModelToolParam& param : tool.m_params)
            {
                properties[param.m_name] = {{"type", param.m_type},
                                            {"description", param.m_description}};
                if (param.m_required)
                {
                    required.push_back(param.m_name);
                }
            }
            nlohmann::json parameters = {{"type", "object"},
                                         {"properties", properties},
                                         {"required", required}};
            tool_array.push_back({{"type", "function"},
                                  {"function", {{"name", tool.m_name},
                                                {"description", tool.m_description},
                                                {"parameters", parameters}}}});
        }
        document["tools"] = tool_array;
        document["tool_choice"] = "auto";
    }

    const std::string body = document.dump();

    return body;
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
    return complete(messages, {});
}

SModelReply CModelOpenAICompat::complete(const std::vector<SModelMessage>& messages,
                                         const std::vector<SModelTool>& tools)
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
        build_headers(), build_body(messages, false, tools), k_timeout_seconds, nullptr);

    if (!http.m_ok)
    {
        reply.m_error = http.m_error;
        return reply;
    }

    try
    {
        const nlohmann::json document = nlohmann::json::parse(http.m_body);
        if (document.contains("choices") && document["choices"].is_array() && !document["choices"].empty())
        {
            const nlohmann::json& choice = document["choices"][0];
            if (choice.contains("finish_reason") && choice["finish_reason"].is_string())
            {
                reply.m_finish_reason = choice["finish_reason"].get<std::string>();
            }
            if (choice.contains("message"))
            {
                const nlohmann::json& message = choice["message"];
                if (message.contains("content") && message["content"].is_string())
                {
                    reply.m_text = message["content"].get<std::string>();
                }
                reply.m_tool_calls = parse_tool_calls(message);
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        reply.m_error = "invalid model json";
        return reply;
    }

    reply.m_ok = !reply.m_text.empty() || !reply.m_tool_calls.empty();
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
        build_headers(), build_body(messages, true, {}), k_timeout_seconds,
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

    if (aborted)
    {
        reply.m_text = std::move(accumulated);
        reply.m_error = "stream callback cancelled output";
        return reply;
    }

    if (!http.m_ok)
    {
        reply.m_error = http.m_error;
        return reply;
    }

    reply.m_text = std::move(accumulated);
    reply.m_ok = !reply.m_text.empty();
    if (!reply.m_ok)
    {
        reply.m_error = "empty streamed response";
    }
    return reply;
}

} // namespace fcpp
