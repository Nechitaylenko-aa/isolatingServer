/**
 * @file CHash.h
 * @brief Встроенный SHA-256 (без внешних зависимостей).
 *
 * Используется для детерминированного вывода идентификатора проекта и пути
 * к базе индекса из канонического пути (ADR-03, Zero-Search).
 */

#ifndef FCPP_AGENT_CHASH_H
#define FCPP_AGENT_CHASH_H

#include <string>

namespace fcpp
{

/**
 * @brief Утилита хэширования SHA-256.
 *
 * Класс без состояния: единственный статический метод вычисляет hex-дайджест.
 */
class CHash
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CHash() = delete;

    /**
     * @brief Вычислить SHA-256 от строки.
     * @param text Входная строка.
     * @return Строка из 64 символов — строчный hex-дайджест.
     */
    static std::string sha256_hex(const std::string& text);
};

} // namespace fcpp

#endif // FCPP_AGENT_CHASH_H
