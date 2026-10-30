/**
 * @file CRequestOrchestrator.h
 * @brief Дерево обработки пользовательского запроса.
 *
 * Оркестратор — единственный владелец маршрута: он решает, какие контексты
 * применимы, заказывает факты и выбирает, какой ответ сформировать. Модель не
 * выбирает ни маршрут, ни инструменты.
 */

#ifndef FCPP_AGENT_CREQUESTORCHESTRATOR_H
#define FCPP_AGENT_CREQUESTORCHESTRATOR_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/CContextBudget.h"
#include "core/CConversationStore.h"
#include "fcpp-agent/IClang.h"
#include "fcpp-agent/IFactProvider.h"
#include "fcpp-agent/SOrchestrationTypes.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{

class CLogger;
class CHistorian;
class CCursorContextAnalyzer;

/**
 * @brief Нормализованный вход от клиентского фасада.
 */
struct SOrchestrationRequest
{
    std::string m_request_id;                        ///< Идентификатор запроса.
    std::string m_text;                              ///< Текст запроса.
    std::string m_session_id;                        ///< Сессия диалога.
    std::string m_cursor_file;                       ///< Файл под курсором.
    std::uint32_t m_cursor_line = 0;                 ///< Строка курсора (0 — нет).
    std::uint32_t m_cursor_column = 0;               ///< Колонка курсора (0 — нет).
    std::vector<std::string> m_cursor_compile_arguments; ///< Аргументы компиляции TU.
    std::vector<SConversationTurn> m_history;        ///< История диалога.
};

/**
 * @brief Результат прохождения дерева.
 */
struct SOrchestrationResult
{
    bool m_ok = false;                              ///< Успех обработки.
    ERequestStatus m_status = ERequestStatus::Failed; ///< Итоговый статус.
    EExitReason m_exit_reason = EExitReason::None;  ///< Причина выхода.
    EOrchestrationRoute m_route = EOrchestrationRoute::Unselected; ///< Маршрут.
    std::vector<ETraceEvent> m_trace;               ///< След прохождения.
    std::string m_message;                          ///< Ответ либо диагностика.
    std::optional<SClangSymbol> m_cursor_symbol;    ///< Сущность под курсором.
    std::string m_cursor_context;                   ///< Срез исходника.
    std::string m_file_context;                     ///< Содержимое файла без курсора.
    std::string m_file_entity;                      ///< Сущность, выбранная моделью.
    std::vector<SConversationTurn> m_history_context;///< Отобранные пары истории.
    std::vector<SFactSection> m_fact_sections;      ///< Факты из индекса.
    SContextBudgetSplit m_budget;                   ///< Распределение бюджета.
};

/**
 * @brief Владелец верхнеуровневого маршрута запроса.
 */
class CRequestOrchestrator
{
public:
    /**
     * @brief Конструктор.
     * @param logger Журнал.
     * @param historian Оценка применимости истории (может быть nullptr).
     * @param cursor_analyzer Оценка курсора (может быть nullptr).
     * @param llm Слой моделей (может быть nullptr).
     * @param facts Поставщик фактов об индексе (может быть nullptr).
     */
    explicit CRequestOrchestrator(CLogger& logger, CHistorian* historian = nullptr,
                                  CCursorContextAnalyzer* cursor_analyzer = nullptr,
                                  CLlmLayer* llm = nullptr,
                                  const IFactProvider* facts = nullptr);

    /**
     * @brief Пройти дерево обработки.
     * @param request Нормализованный запрос.
     * @return Результат с маршрутом, причиной выхода и следом.
     */
    SOrchestrationResult run(const SOrchestrationRequest& request) const;

private:
    /**
     * @brief Исход выбора корня дерева.
     */
    enum class ERootChoice
    {
        Failure,          ///< Сбой оценки контекста.
        CursorAndHistory, ///< Применимы оба контекста.
        Cursor,           ///< Применим только курсор.
        History,          ///< Применима только история.
        File,             ///< Файл без позиции курсора.
        FileAndHistory,   ///< Файл без позиции курсора и история.
        NoContext         ///< Применимых контекстов нет.
    };

    /**
     * @brief Проверить вход и привести его к рабочему виду.
     * @param request Запрос (текст может быть обрезан по краям).
     * @return true если запрос пригоден к обработке.
     */
    bool normalise(SOrchestrationRequest& request) const;

    /**
     * @brief Проверить, что текст запроса влезает в окно любой роли-ответчика.
     *
     * Проверка идёт по минимальному окну среди ролей, которые вообще могут
     * сформировать ответ. Маршрут на этом шаге ещё не выбран, поэтому судить по
     * одной роли нельзя: вопрос, влезающий в удалённую модель, может не влезть
     * в локальную и наоборот.
     *
     * Вызывается до сбора контекста: ответить на слишком длинный запрос всё
     * равно нельзя, а чтение индекса и вызовы моделей стоят времени.
     *
     * @param request Запрос.
     * @param result Результат, куда пишется диагностика.
     * @return true если запрос влезает в окно любой роли-ответчика.
     */
    bool check_request_fits(const SOrchestrationRequest& request,
                           SOrchestrationResult& result) const;

    /**
     * @brief Рассчитать бюджет промпта для роли-ответчика.
     *
     * Вызывается после выбора маршрута, когда роль-ответчик уже известна.
     * Текст запроса входит в контекст целиком (он уже проверен на входе);
     * остаток делится между кодом и историей.
     *
     * @param role Роль, которая сформирует ответ.
     * @param request Запрос.
     * @param result Результат, куда пишется распределение.
     */
    void plan_budget(ELlmRole role, const SOrchestrationRequest& request,
                     SOrchestrationResult& result) const;

    /**
     * @brief Уложить собранный материал в бюджет роли-ответчика.
     *
     * Материал уже нарезан под фазу оценки и лежит локально, поэтому подгонка
     * детерминирована: история обрезается по парам, срез — по границе строк.
     * Повторный вызов модели не выполняется.
     *
     * @param result Результат с собранным материалом.
     * @return true если материал уложен; false при обнаружении нехватки бюджета.
     */
    bool fit_material_to_budget(SOrchestrationResult& result) const;

    /**
     * @brief Оценить применимость курсора, файла и истории независимо.
     *
     * Оценка идёт на собственных лимитах фазы оценки, а не на бюджете роли-
     * ответчика: маршрут в этот момент ещё не выбран, поэтому лимит ответчика
     * неизвестен. Смешивание этих двух лимитов приводило к тому, что материал
     * нарезался под одно окно, а ответ формировала модель с другим.
     *
     * @param request Запрос.
     * @param result Результат, куда пишутся контексты.
     * @return Выбор корня.
     */
    ERootChoice choose_root(const SOrchestrationRequest& request,
                            SOrchestrationResult& result) const;

    /**
     * @brief Заказать факты об индексе по сущности курсора.
     * @param request Запрос.
     * @param result Результат, куда пишутся секции.
     * @return false при недоступности индекса.
     */
    bool collect_facts(const SOrchestrationRequest& request,
                       SOrchestrationResult& result) const;

    /**
     * @brief Выбрать сущность файла и подтвердить её принадлежность файлу.
     *
     * Модель может назвать сущность, но это текст. Принадлежность проверяет
     * индекс. При отказе оркестратор отвечает по содержимому файла целиком:
     * угадывать нельзя, а ошибкой это не является.
     *
     * @param request Запрос.
     * @param result Результат с содержимым файла.
     * @return true если факты по сущности подтверждены.
     */
    bool collect_file_entity_facts(const SOrchestrationRequest& request,
                                   SOrchestrationResult& result) const;

    /**
     * @brief Сформировать ответ по содержимому файла.
     * @param request Запрос.
     * @param result Результат.
     * @param include_history Включать ли отобранную историю.
     */
    void answer_from_file(const SOrchestrationRequest& request,
                          SOrchestrationResult& result,
                          bool include_history) const;

    /**
     * @brief Собрать префикс промпта из среза кода и фактов индекса.
     * @param result Результат с собранными данными.
     * @return Текст префикса.
     */
    std::string build_evidence_block(const SOrchestrationResult& result) const;

    /**
     * @brief Сформировать ответ и зафиксировать его в результате.
     *
     * Единая точка генерации: роль, тип промпта и набор контекста передаются
     * вызывающим. Проверка слоя моделей, логирование сбоя и аудит не
     * дублируются по веткам.
     *
     * @param request Запрос.
     * @param result Результат.
     * @param role Роль модели.
     * @param prompt_type Контракт промпта.
     * @param prompt_input Готовый ввод модели.
     */
    void generate_answer(const SOrchestrationRequest& request,
                         SOrchestrationResult& result,
                         ELlmRole role,
                         EPromptType prompt_type,
                         const std::string& prompt_input) const;

    /**
     * @brief Зафиксировать сбой в результате и в журнале.
     * @param request Запрос.
     * @param result Результат.
     * @param reason Причина.
     * @param message Диагностическое сообщение.
     * @param event Завершающее событие дерева.
     */
    void fail(const SOrchestrationRequest& request,
              SOrchestrationResult& result,
              EExitReason reason,
              const std::string& message,
              ETraceEvent event) const;

    CLogger& m_logger;                              ///< Журнал.
    CHistorian* m_historian = nullptr;              ///< Оценка истории.
    CCursorContextAnalyzer* m_cursor_analyzer = nullptr; ///< Оценка курсора.
    CLlmLayer* m_llm = nullptr;                     ///< Слой моделей.
    const IFactProvider* m_facts = nullptr;         ///< Поставщик фактов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CREQUESTORCHESTRATOR_H
