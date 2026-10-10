/**
 * @file SCascade.h
 * @brief Модель каскада моделей и детекции глюков (ADR-10, Этап 6c).
 */

#ifndef FCPP_AGENT_SCASCADE_H
#define FCPP_AGENT_SCASCADE_H

#include <cstdint>
#include <string>

namespace fcpp
{

/**
 * @brief Логический уровень модели в каскаде (порядок возрастания стоимости).
 */
enum class EModelLevel
{
    Lite,   ///< Локальная лёгкая модель.
    Main,   ///< Локальная основная модель.
    Remote, ///< Удалённая (облачная) модель.
    Count   ///< Маркер количества.
};

/**
 * @brief Тип объективного глюка (детерминированный триггер, ADR-10).
 */
enum class EGlitchKind
{
    None,        ///< Глюка нет.
    Syntax,      ///< Невалидный формат (XML/JSON).
    Reference,   ///< Вымышленная сущность/файл (нет в индексе).
    Compilation, ///< Код не компилируется.
    Loop,        ///< Зацикливание/повтор.
    Schema,      ///< Ответ не соответствует структуре.
    Provider,    ///< Ошибка провайдера/недоступность.
    EmptyReply,  ///< Пустой ответ модели (finish=stop без tool_calls и content).
    Count        ///< Маркер количества.
};

/**
 * @brief Результат одной попытки уровня.
 */
struct SCascadeAttempt
{
    bool m_valid = false;                          ///< Успех попытки.
    EGlitchKind m_glitch = EGlitchKind::None;      ///< Обнаруженный глюк.
    std::string m_result_text;                     ///< Полезный результат.
    std::string m_error_context;                   ///< Контекст ошибки для передачи наверх.
    bool m_stream_started = false;                 ///< Уже выведен фрагмент; повторная попытка испортит ответ.
};

/**
 * @brief Итог работы каскада.
 */
struct SCascadeResult
{
    bool m_ok = false;                          ///< Успех на каком-то уровне.
    EModelLevel m_level = EModelLevel::Lite;    ///< Финальный уровень.
    EGlitchKind m_glitch = EGlitchKind::None;   ///< Последний глюк (при неуспехе).
    std::string m_message;                      ///< Сообщение (при неуспехе — причина).
    std::string m_text;                         ///< Итоговый текст.
    std::uint32_t m_attempts = 0;               ///< Число сделанных попыток.
};

/**
 * @brief Получить машинное имя уровня.
 * @param level Уровень.
 * @return Имя.
 */
const char* model_level_name(EModelLevel level);

/**
 * @brief Получить машинное имя типа глюка.
 * @param glitch Тип.
 * @return Имя.
 */
const char* glitch_kind_name(EGlitchKind glitch);

} // namespace fcpp

#endif // FCPP_AGENT_SCASCADE_H
