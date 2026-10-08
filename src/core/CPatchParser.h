/**
 * @file CPatchParser.h
 * @brief Парсер unified diff (§44).
 *
 * Разбирает текст патча в типизированную модель SPatchSet. Не применяет и не
 * валидирует содержимое — только структура.
 */

#ifndef FCPP_AGENT_CPATCHPARSER_H
#define FCPP_AGENT_CPATCHPARSER_H

#include <string>

#include "fcpp-agent/SPatch.h"

namespace fcpp
{

/**
 * @brief Парсер unified diff.
 */
class CPatchParser
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CPatchParser() = delete;

    /**
     * @brief Разобрать текст патча.
     * @param text Текст unified diff.
     * @param out_set Выходной набор патчей.
     * @return true при успехе; false при ошибке формата.
     */
    static bool parse(const std::string& text, SPatchSet& out_set);
};

} // namespace fcpp

#endif // FCPP_AGENT_CPATCHPARSER_H
