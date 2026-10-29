/**
 * @file SOrchestrationTypes.h
 * @brief Типы состояния дерева обработки запроса.
 *
 * Только объявления: имена значений и их текстовые представления живут в
 * статических массивах в .cpp (Секция 3 rules.md).
 */

#ifndef FCPP_AGENT_SORCHESTRATIONTYPES_H
#define FCPP_AGENT_SORCHESTRATIONTYPES_H

namespace fcpp
{

/**
 * @brief Итоговый статус обработки запроса.
 */
enum class ERequestStatus
{
    Rejected,  ///< Запрос отклонён до выполнения.
    Failed,    ///< Обработка завершилась сбоем.
    Completed, ///< Ответ сформирован.
    Count      ///< Маркер количества.
};

/**
 * @brief Причина терминального исхода.
 */
enum class EExitReason
{
    None,                    ///< Штатное завершение.
    EmptyRequest,            ///< Запрос пуст после нормализации.
    CursorRelevanceFailed,   ///< Оценка курсора завершилась сбоем.
    HistoryRelevanceFailed,  ///< Оценка истории завершилась сбоем.
    FileRelevanceFailed,     ///< Оценка файла без курсора завершилась сбоем.
    FileNotRelevant,         ///< Вопрос не относится к открытому файлу.
    LlmGenerationFailed,     ///< Модель не дала пригодный ответ.
    IndexUnavailable,        ///< База индекса проекта недоступна.
    Count                    ///< Маркер количества.
};

/**
 * @brief Выбранный маршрут обработки.
 */
enum class EOrchestrationRoute
{
    Unselected,                     ///< Маршрут не выбран (сбой до выбора).
    NoApplicableContext,            ///< Ни истории, ни курсора, ни файла.
    CursorContextRequired,          ///< Только курсор.
    HistoryContextRequired,         ///< Только история.
    CursorAndHistoryContextRequired,///< Курсор и история.
    FileContextRequired,            ///< Файл без позиции курсора.
    FileAndHistoryContextRequired,  ///< Файл без позиции курсора и история.
    Count                           ///< Маркер количества.
};

/**
 * @brief Событие прохождения дерева.
 */
enum class ETraceEvent
{
    ValidationOk,                 ///< Вход прошёл нормализацию.
    ValidationFailed,             ///< Вход отклонён.
    ApplicabilityFailed,          ///< Оценка применимости завершилась сбоем.
    ApplicabilityAssessed,        ///< Применимость оценена.
    RouteSelected,                ///< Маршрут выбран.
    AnswerGenerated,              ///< Модель вернула текст.
    AnswerAudited,                ///< Ответ принят.
    ExitLlmGenerationFailed,      ///< Терминальный выход: модель.
    ExitCompleted,                ///< Терминальный выход: успех.
    ExitEmptyRequest,             ///< Терминальный выход: пустой запрос.
    ExitCursorRelevanceFailed,    ///< Терминальный выход: курсор.
    ExitHistoryRelevanceFailed,   ///< Терминальный выход: история.
    ExitFileRelevanceFailed,      ///< Терминальный выход: оценка файла.
    ExitFileNotRelevant,          ///< Терминальный выход: файл не про запрос.
    ExitIndexUnavailable,         ///< Терминальный выход: индекс.
    Count                         ///< Маркер количества.
};

/**
 * @brief Текстовое имя статуса.
 * @param value Статус.
 * @return Стабильное имя.
 */
const char* request_status_name(ERequestStatus value);

/**
 * @brief Текстовое имя причины выхода.
 * @param value Причина.
 * @return Стабильное имя.
 */
const char* exit_reason_name(EExitReason value);

/**
 * @brief Текстовое имя маршрута.
 * @param value Маршрут.
 * @return Стабильное имя.
 */
const char* orchestration_route_name(EOrchestrationRoute value);

/**
 * @brief Текстовое имя события дерева.
 * @param value Событие.
 * @return Стабильное имя.
 */
const char* trace_event_name(ETraceEvent value);

} // namespace fcpp

#endif // FCPP_AGENT_SORCHESTRATIONTYPES_H
