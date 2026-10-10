/**
 * @file CEscalatingIntentAnalyzer.h
 * @brief Анализ Intent с детерминированным каскадом (6c).
 *
 * Пробует lite → main → remote через CModelCascade: при Syntax Fail нижестоящей
 * модели запрос эскалируется с контекстом ошибки (ADR-10).
 */

#ifndef FCPP_AGENT_CESCALATINGINTENTANALYZER_H
#define FCPP_AGENT_CESCALATINGINTENTANALYZER_H

#include <string>
#include <vector>

#include "fcpp-agent/SCascade.h"
#include "fcpp-agent/SConfig.h"
#include "fcpp-agent/SIntent.h"

namespace fcpp
{

/**
 * @brief Анализатор Intent с эскалацией по уровням моделей.
 */
class CEscalatingIntentAnalyzer
{
public:
    /**
     * @brief Конструктор.
     * @param config Конфигурация (модели).
     */
    explicit CEscalatingIntentAnalyzer(const SConfig& config);

    /**
     * @brief Деструктор.
     */
    ~CEscalatingIntentAnalyzer() = default;

    /**
     * @brief Проанализировать запрос с эскалацией lite→main→remote.
     * @param user_text Текст запроса.
     * @param cursor_hint Хинт символа под курсором (может быть пустой).
     * @param out_intent Выходной Intent.
     * @param out_error Сообщение об ошибке.
     * @return true при успехе.
     */
    bool analyze(const std::string& user_text,
                 const std::string& cursor_hint,
                 SIntent& out_intent,
                 std::string& out_error) const;

private:
    /**
     * @brief Уровни моделей, реально доступные по конфигурации.
     * @return Список уровней по возрастанию стоимости.
     */
    std::vector<EModelLevel> available_levels() const;

    SConfig m_config; ///< Конфигурация.
};

} // namespace fcpp

#endif // FCPP_AGENT_CESCALATINGINTENTANALYZER_H
