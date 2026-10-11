/**
 * @file IAgentApi.h
 * @brief Типизированный Agent API (§89, §93).
 *
 * CLI, IDE и GUI являются равноправными клиентами этого API. API не раскрывает
 * внутренние сущности (Intent/Area/Relation/Tool) как обязательные (§89.39).
 */

#ifndef FCPP_AGENT_IAGENTAPI_H
#define FCPP_AGENT_IAGENTAPI_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "fcpp-agent/IIndex.h"
#include "fcpp-agent/SError.h"

namespace fcpp
{

/**
 * @brief Метод Agent API.
 *
 * Преобразование в текст — статическим массивом в .cpp (Секция 3).
 */
enum class EApiMethod
{
    AgentStatus,     ///< Состояние агента.
    AgentCapabilities,///< Список методов/событий.
    ConfigReload,    ///< Перечитать agent.conf на лету.
    ProjectOpen,     ///< Открыть проект.
    ProjectClose,    ///< Закрыть проект.
    ProjectStatus,   ///< Состояние проекта.
    ProjectList,     ///< Список проектов.
    IndexStatus,     ///< Состояние индекса.
    IndexRefresh,    ///< Инкрементальное обновление.
    IndexRebuild,    ///< Полная перестройка.
    Build,           ///< Сборка (cmake --build).
    Tests,           ///< Тесты (ctest).
    Tool,            ///< Детерминированный запрос к индексу.
    Patch,           ///< Применение патча.
    Doctor,          ///< Проверка окружения.
    Ask,             ///< Пользовательский AI-запрос (§89.13, §94).
    SessionOpen,     ///< Создать conversation session.
    SessionClose,    ///< Закрыть conversation session.
    SessionUpdateContext, ///< Обновить структурированный контекст сессии.
    ProjectProfileSet, ///< Установить паспорт текущего проекта.
    QuestionShrink,    ///< Обрезать диалог до указанного вопроса (пока заглушка).
    Count            ///< Маркер количества.
};

/**
 * @brief Один вопрос, отображаемый пользователю.
 *
 * options пуст — свободный текстовый ответ; иначе пользователь выбирает один
 * из перечисленных вариантов. m_and зарезервирован для будущей группировки.
 */
struct SQuestion
{
    std::uint16_t m_id = 0;
    std::string m_text;
    std::vector<std::string> m_options;
    bool m_and = false;
};

/**
 * @brief Структурированный запрос агента к пользователю.
 *
 * Текущая версия выдаёт один вопрос за раз. Вектор оставлен в модели для
 * расширения до группы вопросов без смены API-формата.
 */
struct SChatQuestion
{
    std::vector<SQuestion> m_questions;
    bool m_wait_for_user = true;
};

/** @brief Тип записи в линейной истории диалога. */
enum class EChatItemType : std::uint8_t
{
    UserRequest,
    AgentQuestion,
    UserAnswer,
    AgentResponse
};

/**
 * @brief Одна запись линейной истории диалога.
 *
 * m_text содержит пользовательский запрос/ответ либо итоговый ответ агента.
 * Для AgentQuestion структурированные данные находятся в m_question.
 * Действия агента намеренно не являются типом записи диалога.
 */
struct SChatItem
{
    std::uint16_t m_id = 0;
    EChatItemType m_type = EChatItemType::UserRequest;
    std::string m_text;
    SChatQuestion m_question;
};

using TChat = std::vector<SChatItem>;

/**
 * @brief Типизированный запрос к Agent API.
 */
struct SApiRequest
{
    EApiMethod m_method = EApiMethod::AgentStatus; ///< Метод.
    std::string m_path;   ///< Путь (проект, патч, файл).
    std::string m_project_root; ///< Корень проекта для шага open (protocol-имя projectRoot).
    std::string m_target; ///< Цель (символ/файл) для Tool.
    std::string m_tool;   ///< Имя инструмента для Tool.
    std::string m_payload;///< Данные (например, текст патча).
    bool m_assume_yes = false; ///< Автоподтверждение (patch).
    bool m_build_after = false;///< Проверить сборкой после patch (COMPILATION FAIL → откат).
    std::uint32_t m_limit = 0; ///< Лимит результатов (0 — без лимита).

    // Тело запроса Ask (§94): только текст и опциональный редакторский контекст.
    std::string m_text;               ///< Текст запроса.
    std::uint16_t m_answer_question_id = 0; ///< ID вопроса, на который отвечает пользователь.
    std::string m_selected_option;     ///< Выбранный вариант ответа.
    std::uint16_t m_question_id = 0;   ///< Вопрос для question.shrink.
    std::string m_session_id;         ///< Идентификатор диалога (опционален для ask).
    std::string m_context_json;       ///< Контекст для session.update_context (JSON).
    std::string m_profile_text;       ///< Текст паспорта для project.profile.set.
    std::string m_editor_file;        ///< Файл редактора (может быть пустым).
    std::uint32_t m_editor_line = 0;  ///< Строка курсора (0 — нет).
    std::uint32_t m_editor_column = 0;///< Колонка курсора (0 — нет).
    bool m_stream = false;            ///< Режим вывода (транспорт CLI, не семантика запроса).
    std::function<bool(const std::string&)> m_on_stream_chunk; ///< Потоковый вывод; false прерывает генерацию.
};

/**
 * @brief Типизированный ответ Agent API.
 */
struct SApiResponse
{
    bool m_ok = false;                     ///< Успех.
    EErrorKind m_error = EErrorKind::Ok;   ///< Категория ошибки.
    std::string m_message;                 ///< Сообщение.
    std::string m_state;                   ///< Состояние для протокола (READY/RUNNING/COMPLETED).
    std::string m_project_id;              ///< Идентификатор проекта.
    std::string m_session_id;              ///< Идентификатор conversation session.
    std::string m_text;                    ///< Текстовый вывод (source/сообщение).
    bool m_has_question = false;           ///< Есть структурированный вопрос для пользователя.
    SChatQuestion m_question;              ///< Вопрос и варианты выбора.
    bool m_has_shrink_result = false;      ///< Ответ содержит результат shrink_question.
    bool m_shrink_result = false;          ///< false означает, что чат не изменён.
    SIndexStatus m_index_status;           ///< Состояние индекса.
    std::vector<SSymbol> m_symbols;        ///< Сущности (для Tool).
    std::vector<std::string> m_names;      ///< Имена (списки).
    std::vector<SSymbolLocation> m_locations;///< Позиции (для Tool).
};

/**
 * @brief Интерфейс Agent API.
 */
class IAgentApi
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~IAgentApi() = default;

    /**
     * @brief Выполнить запрос.
     * @param request Запрос.
     * @return Ответ.
     */
    virtual SApiResponse execute(const SApiRequest& request) = 0;
};

/**
 * @brief Получить машинное имя метода API.
 * @param method Метод.
 * @return Стабильное текстовое имя.
 */
const char* api_method_name(EApiMethod method);

/**
 * @brief Проверить, является ли текст именем детерминированного инструмента.
 * @param text Текст команды CLI.
 * @return true если это известный инструмент.
 */
bool api_is_tool_command(const std::string& text);

} // namespace fcpp

#endif // FCPP_AGENT_IAGENTAPI_H
