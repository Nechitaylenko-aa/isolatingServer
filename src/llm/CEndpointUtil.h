/**
 * @file CEndpointUtil.h
 * @brief Утилита сборки endpoint URL из base_url (6b).
 *
 * Конфиг требует base_url до /v1 включительно; код добавляет путь API
 * (например /chat/completions). Никакого угадывания.
 */

#ifndef FCPP_AGENT_CENDPOINTUTIL_H
#define FCPP_AGENT_CENDPOINTUTIL_H

#include <string>

namespace fcpp
{

/**
 * @brief Утилита сборки URL.
 */
class CEndpointUtil
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CEndpointUtil() = delete;

    /**
     * @brief Собрать полный URL: base_url + path.
     * @param base_url База (до /v1 включительно).
     * @param path Путь API (например "/chat/completions").
     * @return Полный URL (без двойных слэшей).
     */
    static std::string join(const std::string& base_url, const std::string& path);
};

} // namespace fcpp

#endif // FCPP_AGENT_CENDPOINTUTIL_H
