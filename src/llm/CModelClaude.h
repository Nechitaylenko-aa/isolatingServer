/**
 * @file CModelClaude.h
 * @brief Провайдер Anthropic Claude API (6b).
 *
 * base_url из конфига содержит путь до /v1 включительно; провайдер добавляет
 * /messages. Использует заголовки x-api-key и anthropic-version.
 */

#ifndef FCPP_AGENT_CMODELCLAUDE_H
#define FCPP_AGENT_CMODELCLAUDE_H

#include <string>
#include <vector>

#include "fcpp-agent/IModelApi.h"

namespace fcpp
{

/**
 * @brief Провайдер Claude.
 */
class CModelClaude final : public IModelApi
{
public:
    /**
     * @brief Конструктор.
     * @param endpoint Конечная точка (base_url/model/key).
     * @param temperature Температура.
     * @param max_tokens Лимит токенов.
     */
    CModelClaude(SModelEndpoint endpoint, double temperature, int max_tokens);

    /**
     * @brief Деструктор.
     */
    ~CModelClaude() override = default;

    /**
     * @brief Проверить готовность провайдера.
     * @return true если base_url и модель заданы.
     */
    bool is_available() const override;

    /**
     * @brief Синхронный запрос.
     * @param messages Сообщения.
     * @return Ответ.
     */
    SModelReply complete(const std::vector<SModelMessage>& messages) override;

    /**
     * @brief Синхронный запрос с инструментами (Claude tool_use).
     *
     * Полноценный tool_use: схемы передаются в поле `tools`, tool_calls
     * возвращаются в ответе (`content[]` с type=tool_use). Это делает Claude
     * пригодным как ведущая модель ReAct-цикла.
     * @param messages Сообщения.
     * @param tools Схемы инструментов.
     * @return Ответ.
     */
    SModelReply complete(const std::vector<SModelMessage>& messages,
                         const std::vector<SModelTool>& tools) override;

    /**
     * @brief Потоковый запрос (SSE).
     * @param messages Сообщения.
     * @param on_chunk Callback порций.
     * @return Ответ.
     */
    SModelReply complete_stream(const std::vector<SModelMessage>& messages,
                                const SStreamCallback& on_chunk) override;

private:
    /**
     * @brief Собрать JSON-тело запроса Claude.
     * @param messages Сообщения.
     * @param stream Признак потока.
     * @param tools Схемы инструментов (пусто — без tools).
     * @return JSON-строка.
     */
    std::string build_body(const std::vector<SModelMessage>& messages,
                           bool stream,
                           const std::vector<SModelTool>& tools) const;

    /**
     * @brief Собрать заголовки (x-api-key и версия).
     * @return Заголовки.
     */
    std::vector<std::string> build_headers() const;

    SModelEndpoint m_endpoint; ///< Конечная точка.
    double m_temperature;      ///< Температура.
    int m_max_tokens;          ///< Лимит токенов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CMODELCLAUDE_H
