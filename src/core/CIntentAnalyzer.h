/**
 * @file CIntentAnalyzer.h
 * @brief Анализ пользовательского текста в Intent через lite-модель (6b).
 *
 * LLM лишь извлекает Intent (§29); детерминированное ядро валидирует и
 * маршрутизирует. Парсинг — тег-парсер (не regex), значения валидируются
 * по enum (Секция 3).
 */

#ifndef FCPP_AGENT_CINTENTANALYZER_H
#define FCPP_AGENT_CINTENTANALYZER_H

#include <string>

#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/SIntent.h"

namespace fcpp
{

/**
 * @brief Анализатор Intent.
 */
class CIntentAnalyzer
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CIntentAnalyzer() = default;

    /**
     * @brief Деструктор.
     */
    ~CIntentAnalyzer() = default;

    /**
     * @brief Проанализировать текст пользователя через модель.
     * @param model Провайдер lite-модели.
     * @param user_text Текст запроса.
     * @param cursor_hint Готовый хинт символа под курсором (может быть пустой).
     * @param out_intent Выходной Intent.
     * @param out_error Сообщение об ошибке.
     * @return true при успехе разбора.
     */
    bool analyze(IModelApi& model,
                 const std::string& user_text,
                 const std::string& cursor_hint,
                 SIntent& out_intent,
                 std::string& out_error) const;

    /**
     * @brief Разобрать XML-ответ модели в Intent (без сети).
     * @param xml Текст ответа.
     * @param out_intent Выходной Intent.
     * @param out_error Сообщение об ошибке.
     * @return true при успехе.
     */
    static bool parse_intent_xml(const std::string& xml, SIntent& out_intent, std::string& out_error);
};

} // namespace fcpp

#endif // FCPP_AGENT_CINTENTANALYZER_H
