/**
 * @file CModelOpenAICompat.h
 * @brief Провайдер OpenAI-совместимого API (6b).
 *
 * base_url из конфига содержит путь до /v1 включительно; провайдер добавляет
 * /chat/completions. Поддерживает синхронный и потоковый (SSE) режимы.
 */

#ifndef FCPP_AGENT_CMODELOPENAICOMPAT_H
#define FCPP_AGENT_CMODELOPENAICOMPAT_H

#include <string>
#include <vector>

#include "fcpp-agent/IModelApi.h"

namespace fcpp
{

/**
 * @brief Провайдер OpenAI-compatible API.
 */
class CModelOpenAICompat final : public IModelApi
{
public:
    /**
     * @brief Конструктор.
     * @param endpoint Конечная точка (base_url/model/key).
     * @param temperature Температура генерации.
     * @param max_tokens Лимит токенов ответа.
     */
    CModelOpenAICompat(SModelEndpoint endpoint, double temperature, int max_tokens);

    /**
     * @brief Деструктор.
     */
    ~CModelOpenAICompat() override = default;

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
     * @brief Синхронный запрос с описанием инструментов (ReAct).
     * @param messages Сообщения.
     * @param tools Схемы инструментов.
     * @return Ответ (может содержать tool_calls).
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
     * @brief Собрать JSON-тело запроса.
     * @param messages Сообщения.
     * @param stream Признак потокового режима.
     * @param tools Схемы инструментов (пусто — без tools).
     * @return JSON-строка.
     */
    std::string build_body(const std::vector<SModelMessage>& messages,
                           bool stream,
                           const std::vector<SModelTool>& tools) const;

    /**
     * @brief Собрать заголовки запроса (включая ключ).
     * @return Заголовки.
     */
    std::vector<std::string> build_headers() const;

    /**
     * @brief Извлечь текст ответа из JSON (не потоковый режим).
     * @param body Тело ответа.
     * @return Текст либо пустая строка.
     */
    static std::string extract_content(const std::string& body);

    SModelEndpoint m_endpoint; ///< Конечная точка.
    double m_temperature;      ///< Температура.
    int m_max_tokens;          ///< Лимит токенов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CMODELOPENAICOMPAT_H
