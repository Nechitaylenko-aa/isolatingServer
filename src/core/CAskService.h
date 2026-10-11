/**
 * @file CAskService.h
 * @brief Оркестратор `ask`: lite→Intent→pipeline→evidence→main (6b).
 *
 * LLM используется только для интерпретации (lite) и формулировки ответа (main).
 * Резолв/план/политика — детерминированные (CRequestPipeline).
 */

#ifndef FCPP_AGENT_CASKSERVICE_H
#define FCPP_AGENT_CASKSERVICE_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "core/CConversationStore.h"
#include "fcpp-agent/IClang.h"
#include "fcpp-agent/SIntent.h"

namespace fcpp
{

/**
 * @brief Сервис обработки пользовательского запроса (ask).
 */
class CAskService
{
public:
    /**
     * @brief Конструктор.
     * @param config_manager Конфигурация.
     * @param project_manager Менеджер проектов.
     */
    CAskService(const CConfigManager& config_manager, CProjectManager& project_manager);

    /**
     * @brief Деструктор.
     */
    ~CAskService() = default;

    /**
     * @brief Выполнить запрос пользователя.
     *
     * Ответ возвращается в `out_answer` (для протокола/JSON). Диагностика
     * («Понял так», декомпозиция, ошибки шагов) идёт в stderr и не засоряет stdout.
     * @param text Текст запроса.
     * @param stream Дополнительно печатать поток ответа модели в stdout (CLI).
     * @param cursor_file Текущий файл редактора (опционально).
     * @param cursor_line Строка курсора (0 — нет).
     * @param cursor_col Колонка курсора (0 — нет).
     * @param out_answer Выходной текст ответа (или уточнение/сообщение).
     * @return true при успехе.
     */
    bool ask(const std::string& text,
             bool stream,
             const std::string& cursor_file,
             unsigned cursor_line,
             unsigned cursor_col,
             std::string& out_answer,
             const std::string& context_json = "{}",
             const std::vector<SConversationTurn>& history = {},
             const std::function<bool(const std::string&)>& on_stream_chunk = {}) const;

private:
    /**
     * @brief Разрешить курсор в сущность через IClang (innermost, B2/ADR-20).
     *
     * Возвращает то, что вернул `cursor_at` без фильтра вида — имя и категорию
     * подставляются в промпт lite, а применимость решает сама модель.
     * @param file Файл.
     * @param line Строка.
     * @param col Колонка.
     * @return Символ либо пустое значение.
     */
    std::optional<SClangSymbol> cursor_symbol(const std::string& file,
                                              unsigned line,
                                              unsigned col) const;

    /**
     * @brief Определить якорь иерархии — базовый класс по позиции курсора (ADR-18 №0).
     *
     * Навигация по наследованию через индекс (`ancestors`): если база достижима —
     * вернуть её имя, иначе пустую строку (fallback — якорь = курсорная сущность).
     * @param file Файл.
     * @param line Строка.
     * @param col Колонка.
     * @return Имя базового класса либо пустая строка.
     */
    std::string hierarchy_anchor(const std::string& file, unsigned line, unsigned col) const;

    const CConfigManager& m_config_manager; ///< Конфигурация.
    CProjectManager& m_project_manager;     ///< Менеджер проектов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CASKSERVICE_H
