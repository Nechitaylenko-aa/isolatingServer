/**
 * @file IModelApi.h
 * @brief Интерфейс провайдера LLM-модели (6b).
 *
 * Провайдер — «тупой транспорт»: prompt/streaming/cache. Он НЕ решает intent и
 * НЕ управляет tools — это делает детерминированное ядро.
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
    Count
};

/**
 * @brief Сообщение диалога.
 */
struct SModelMessage
{
    EModelRole m_role = EModelRole::User; ///< Роль.
    std::string m_content;                ///< Текст.
};

/**
 * @brief Ответ модели.
 */
struct SModelReply
{
    bool m_ok = false;      ///< Успех.
    std::string m_text;     ///< Сгенерированный текст.
    std::string m_error;    ///< Сообщение об ошибке (без секретов).
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
     * @brief Потоковый запрос к модели.
     * @param messages Сообщения.
     * @param on_chunk Callback порций (возврат false прерывает).
     * @return Ответ.
     */
    virtual SModelReply complete_stream(const std::vector<SModelMessage>& messages,
                                        const SStreamCallback& on_chunk) = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_IMODELAPI_H
