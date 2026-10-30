#ifndef FCPP_AGENT_CHISTORIAN_H
#define FCPP_AGENT_CHISTORIAN_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "core/CConversationStore.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{
struct SHistoryRelevance
{
    bool m_ok = false;
    bool m_history_needed = false;
    bool m_escalated = false;
    std::string m_error;
    ELlmRole m_used_role = ELlmRole::Count;
    std::vector<std::size_t> m_relevant_turn_indices;
    std::size_t m_considered_turns = 0; ///< Сколько хвостовых пар попало в запрос.
    std::size_t m_dropped_turns = 0;    ///< Сколько старых пар отброшено по лимиту.
};

/**
 * @brief Оценка применимости истории диалога.
 */
class CHistorian
{
public:
    /**
     * @brief Конструктор.
     * @param llm Слой моделей.
     */
    explicit CHistorian(CLlmLayer& llm);

    /**
     * @brief Оценить, какие прошлые пары нужны для текущего запроса.
     *
     * История ограничивается по числу пар и по размеру: в модель уходит только
     * хвост диалога, старые пары отбрасываются. Нумерация в запросе остаётся
     * непрерывной (1..N по отправленному хвосту), а возвращённые индексы
     * пересчитываются в исходные позиции истории.
     *
     * Ограничение обязательно: обрезка промпта по байтам в LLM-слое ломает
     * нумерованный список, и модель возвращает номера, не соответствующие
     * содержимому.
     *
     * @param current_request Текст запроса.
     * @param history Полная история диалога.
     * @return Результат оценки с индексами в исходной истории.
     */
    SHistoryRelevance assess_relevance(
        const std::string& current_request,
        const std::vector<SConversationTurn>& history) const;

    /**
     * @brief Оценить применимость истории в пределах заданного лимита.
     *
     * Лимит приходит из общего бюджета промпта (доля истории), поэтому история
     * не может вместе с кодом превысить окно модели.
     *
     * @param current_request Текст запроса.
     * @param history Полная история диалога.
     * @param max_bytes Бюджет истории в байтах; 0 — значение по умолчанию.
     * @return Результат оценки с индексами в исходной истории.
     */
    SHistoryRelevance assess_relevance(
        const std::string& current_request,
        const std::vector<SConversationTurn>& history,
        std::size_t max_bytes) const;

    /**
     * @brief Разобрать номера пар из ответа модели.
     * @param text Ответ модели.
     * @param turn_count Сколько пар было предложено.
     * @param indices Выходные 0-based индексы.
     * @param error Причина отказа разбора.
     * @return true если ответ корректен.
     */
    static bool decode_relevant_indices(
        const std::string& text, std::size_t turn_count,
        std::vector<std::size_t>& indices, std::string& error);

private:
    CLlmLayer& m_llm; ///< Слой моделей.
};
} // namespace fcpp

#endif
