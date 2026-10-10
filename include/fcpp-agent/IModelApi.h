/**
 * @file IModelApi.h
 * @brief Интерфейс провайдера LLM-модели (6b).
 *
 * Провайдер — транспорт prompt/streaming/cache. Он НЕ решает intent и НЕ сам
 * исполняет tools: при ReAct-режиме модель лишь ВОЗВРАЩАЕТ tool_calls, а
 * исполняет их детерминированное ядро (CReActAgent + CToolRegistry).
 */

#ifndef FCPP_AGENT_IMODELAPI_H
#define FCPP_AGENT_IMODELAPI_H

#include <functional>
#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Роль сообщения в диалоге.
 */
enum class EModelRole
{
    System,
    User,
    Assistant,
    Tool,
    Count
};

/**
 * @brief Вызов инструмента, запрошенный моделью.
 */
struct SModelToolCall
{
    std::string m_id{};        ///< Идентификатор вызова (для role=tool).
    std::string m_name{};      ///< Имя инструмента.
    std::string m_arguments{}; ///< Аргументы (строка JSON).
};

/**
 * @brief Сообщение диалога.
 */
struct SModelMessage
{
    EModelRole m_role = EModelRole::User;     ///< Роль.
    std::string m_content{};                  ///< Текст.
    std::vector<SModelToolCall> m_tool_calls{}; ///< Вызовы (для Assistant в ReAct).
    std::string m_tool_call_id{};             ///< id вызова (для роли Tool).
};

/**
 * @brief Параметр инструмента (схема для модели).
 */
struct SModelToolParam
{
    std::string m_name;        ///< Имя параметра.
    std::string m_type;        ///< JSON-тип (string/integer/...).
    std::string m_description; ///< Описание.
    bool m_required = true;    ///< Обязателен ли.
};

/**
 * @brief Схема инструмента (описание для модели в режиме ReAct).
 */
struct SModelTool
{
    std::string m_name;                     ///< Имя инструмента.
    std::string m_description;              ///< Что делает (для модели).
    std::vector<SModelToolParam> m_params;  ///< Параметры.
};

/**
 * @brief Ответ модели.
 */
struct SModelReply
{
    bool m_ok = false;                         ///< Успех.
    std::string m_text;                        ///< Сгенерированный текст.
    std::string m_error;                       ///< Сообщение об ошибке (без секретов).
    std::string m_finish_reason;               ///< stop/tool_calls/length/...
    std::vector<SModelToolCall> m_tool_calls;  ///< Запрошенные вызовы (ReAct).
};

/**
 * @brief Описание конечной точки модели (из конфига).
 */
struct SModelEndpoint
{
    std::string m_base_url; ///< Base URL (до /v1 включительно).
    std::string m_model;    ///< Имя модели.
    std::string m_api_key;  ///< API-ключ (секрет, не логируется).
};

/**
 * @brief Callback порции потокового ответа.
 * @param chunk Порция текста.
 * @return false для прерывания генерации.
 */
using SStreamCallback = std::function<bool(const std::string& chunk)>;

/**
 * @brief Интерфейс провайдера модели.
 */
class IModelApi
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~IModelApi() = default;

    /**
     * @brief Проверить готовность провайдера (base_url/model заданы).
     * @return true если можно отправлять запросы.
     */
    virtual bool is_available() const = 0;

    /**
     * @brief Синхронный запрос к модели.
     * @param messages Сообщения.
     * @return Ответ.
     */
    virtual SModelReply complete(const std::vector<SModelMessage>& messages) = 0;

    /**
     * @brief Синхронный запрос с описанием доступных инструментов (ReAct).
     *
     * Провайдер передаёт схемы модели и разбирает `tool_calls` из ответа.
     * @param messages Сообщения.
     * @param tools Схемы инструментов.
     * @return Ответ (может содержать `m_tool_calls`).
     */
    virtual SModelReply complete(const std::vector<SModelMessage>& messages,
                                 const std::vector<SModelTool>& tools) = 0;

    /**
     * @brief Потоковый запрос к модели.
     * @param messages Сообщения.
     * @param on_chunk Callback порций (возврат false прерывает).
     * @return Ответ.
     */
    virtual SModelReply complete_stream(const std::vector<SModelMessage>& messages,
                                        const SStreamCallback& on_chunk) = 0;

    // Tool-enabled responses are buffered until tool_calls are known, preventing
    // internal narration from leaking. Use the no-tools overload for live final output.
    virtual SModelReply complete_stream(const std::vector<SModelMessage>& messages,
                                        const std::vector<SModelTool>& tools,
                                        const SStreamCallback& on_chunk)
    {
        SModelReply reply = complete(messages, tools);
        if (reply.m_ok && reply.m_tool_calls.empty() && !reply.m_text.empty()
                && on_chunk && !on_chunk(reply.m_text))
        {
            reply.m_ok = false;
            reply.m_error = "stream callback cancelled output";
        }
        return reply;
    }
};

} // namespace fcpp

#endif // FCPP_AGENT_IMODELAPI_H
