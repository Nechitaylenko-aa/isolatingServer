/**
 * @file CUrlTransport.h
 * @brief HTTP-транспорт на libcurl (6b).
 *
 * Единая точка сетевого вызова. Секреты (API-ключ) не логируются (§61).
 */

#ifndef FCPP_AGENT_CURLTRANSPORT_H
#define FCPP_AGENT_CURLTRANSPORT_H

#include <functional>
#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Результат HTTP-запроса.
 */
struct SHttpResult
{
    bool m_ok = false;      ///< Успех (сеть + HTTP 2xx).
    long m_status = 0;      ///< HTTP-код.
    std::string m_body;     ///< Тело ответа.
    std::string m_error;    ///< Ошибка (без секретов).
};

/**
 * @brief HTTP-транспорт.
 */
class CUrlTransport
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CUrlTransport() = default;

    /**
     * @brief Деструктор.
     */
    ~CUrlTransport() = default;

    /**
     * @brief Выполнить POST с JSON-телом.
     * @param url URL.
     * @param headers Заголовки (например "Authorization: Bearer ...").
     * @param json_body Тело запроса.
     * @param timeout_seconds Таймаут.
     * @param on_chunk Callback потоковых порций тела (nullptr — без стриминга).
     * @return Результат.
     */
    SHttpResult post_json(const std::string& url,
                          const std::vector<std::string>& headers,
                          const std::string& json_body,
                          long timeout_seconds,
                          const std::function<bool(const std::string&)>& on_chunk) const;
};

} // namespace fcpp

#endif // FCPP_AGENT_CURLTRANSPORT_H
