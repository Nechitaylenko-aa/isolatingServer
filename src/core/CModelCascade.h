/**
 * @file CModelCascade.h
 * @brief Детерминированный каскад моделей (ADR-10, Этап 6c).
 *
 * Правила (зафиксированы в ADR-10 п.5):
 *  - решение об эскалации принимает детерминированный детектор (не LLM);
 *  - на каждом уровне ровно 1 авто-ретрай с добавлением ошибки в контекст;
 *  - жёсткий бюджет попыток (без бесконечных циклов);
 *  - при исчерпании — hard-stop с причиной.
 *
 * Класс НЕ знает про сеть: логика попытки передаётся коллбэком (attempt),
 * что делает каскад полностью тестируемым без моделей.
 */

#ifndef FCPP_AGENT_CMODELCASCADE_H
#define FCPP_AGENT_CMODELCASCADE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "fcpp-agent/SCascade.h"

namespace fcpp
{

/**
 * @brief Исполнитель каскада моделей.
 */
class CModelCascade
{
public:
    /**
     * @brief Функция одной попытки.
     *
     * @param level Уровень модели.
     * @param extra_context Контекст ошибки предыдущей попытки (пусто на первой).
     * @return Результат попытки.
     */
    using AttemptFn = std::function<SCascadeAttempt(EModelLevel level, const std::string& extra_context)>;

    /**
     * @brief Конструктор по умолчанию.
     */
    CModelCascade() = delete;

    /**
     * @brief Выполнить каскад по списку доступных уровней.
     * @param levels Доступные уровни по возрастанию стоимости (например {Lite, Main, Remote}).
     * @param attempt Функция попытки.
     * @param max_attempts Жёсткий бюджет попыток (защита от циклов).
     * @return Итог каскада.
     */
    static SCascadeResult run(const std::vector<EModelLevel>& levels,
                              const AttemptFn& attempt,
                              std::uint32_t max_attempts);
};

} // namespace fcpp

#endif // FCPP_AGENT_CMODELCASCADE_H
