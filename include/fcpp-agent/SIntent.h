/**
 * @file SIntent.h
 * @brief Intent IR — стабильная таксономия пользовательского запроса (§18–§28).
 *
 * Пять независимых измерений: Area × Subject × Relation × Projection × Effect.
 * Никакого «гигантского enum всех запросов» (§25).
 */

#ifndef FCPP_AGENT_SINTENT_H
#define FCPP_AGENT_SINTENT_H

#include <cstdint>
#include <string>
#include <vector>

#include "fcpp-agent/ITool.h"

namespace fcpp
{

/**
 * @brief Область задачи (§19).
 */
enum class EArea
{
    Understand, ///< Понимание кода.
    Diagnose,   ///< Поиск причины проблемы.
    Design,     ///< Проектирование решения.
    Change,     ///< Изменение проекта.
    Verify,     ///< Выполнение проверок (build/tests).
    Review,     ///< Оценка собранной информации.
    Env,        ///< Работа с окружением.
    Count       ///< Маркер количества.
};

/**
 * @brief Тип субъекта запроса (§20).
 */
enum class ESubjectKind
{
    Project,
    File,
    Namespace,
    Class,
    Struct,
    Enum,
    Function,
    Method,
    Variable,
    Field,
    Symbol,
    BuildTarget,
    Count
};

/**
 * @brief Способ выбора субъекта (§20).
 */
enum class ESubjectSelection
{
    ExplicitName,
    Cursor,
    CurrentFile,
    CurrentProject,
    Context,
    Inferred,
    Count
};

/**
 * @brief Отношение субъекта к другим сущностям (§23).
 */
enum class ERelation
{
    Self,
    Callers,
    Callees,
    Overrides,
    References,
    Includes,
    Count
};

/**
 * @brief Что пользователь хочет получить (§24).
 */
enum class EProjection
{
    Signature,
    Source,
    Explanation,
    Locations,
    Graph,
    Count
};

/**
 * @brief Реальное воздействие операции (§26).
 *
 * Порядок возрастания опасности: ReadOnly < WriteSource < Execute < WriteBuild.
 */
enum class EEffect
{
    ReadOnly,
    WriteSource,
    Execute,
    WriteBuild,
    Count
};

/**
 * @brief Подтип изменения (§19 CHANGE).
 */
enum class EChangeKind
{
    Modify,
    Create,
    Remove,
    Count
};

/**
 * @brief Субъект запроса.
 */
struct SSubject
{
    ESubjectKind m_kind = ESubjectKind::Symbol;               ///< Тип.
    ESubjectSelection m_selection = ESubjectSelection::ExplicitName; ///< Способ выбора.
    std::string m_name;                                       ///< Имя/цель.
};

/**
 * @brief Структурированное намерение пользователя (§28).
 */
struct SIntent
{
    EArea m_area = EArea::Understand;                 ///< Область.
    SSubject m_subject;                               ///< Субъект.
    ERelation m_relation = ERelation::Self;           ///< Отношение.
    EProjection m_projection = EProjection::Explanation; ///< Проекция.
    EEffect m_effect = EEffect::ReadOnly;             ///< Воздействие.
    EChangeKind m_change_kind = EChangeKind::Modify;  ///< Подтип изменения (для CHANGE).
    std::uint32_t m_confidence = 0;                   ///< Уверенность (0–100).
};

/**
 * @brief Детерминированный план получения данных (§30).
 */
struct SPlan
{
    std::vector<EToolName> m_tools; ///< Последовательность инструментов.
    bool m_requires_write = false;  ///< Требуется ли запись исходников.
    bool m_requires_exec = false;   ///< Требуется ли выполнение команд.
};

/**
 * @brief Получить машинное имя области.
 * @param area Область.
 * @return Имя.
 */
const char* area_name(EArea area);

/**
 * @brief Получить машинное имя типа субъекта.
 * @param kind Тип.
 * @return Имя.
 */
const char* subject_kind_name(ESubjectKind kind);

/**
 * @brief Получить машинное имя отношения.
 * @param relation Отношение.
 * @return Имя.
 */
const char* relation_name(ERelation relation);

/**
 * @brief Получить машинное имя проекции.
 * @param projection Проекция.
 * @return Имя.
 */
const char* projection_name(EProjection projection);

/**
 * @brief Получить машинное имя воздействия.
 * @param effect Воздействие.
 * @return Имя.
 */
const char* effect_name(EEffect effect);

/**
 * @brief Разобрать область из строки.
 * @param text Текст.
 * @param out Выходная область.
 * @return true если распознано.
 */
bool parse_area(const std::string& text, EArea& out);

/**
 * @brief Разобрать отношение из строки.
 * @param text Текст.
 * @param out Выходное отношение.
 * @return true если распознано.
 */
bool parse_relation(const std::string& text, ERelation& out);

/**
 * @brief Разобрать проекцию из строки.
 * @param text Текст.
 * @param out Выходная проекция.
 * @return true если распознано.
 */
bool parse_projection(const std::string& text, EProjection& out);

/**
 * @brief Разобрать тип субъекта из строки.
 * @param text Текст.
 * @param out Выходной тип.
 * @return true если распознано.
 */
bool parse_subject_kind(const std::string& text, ESubjectKind& out);

} // namespace fcpp

#endif // FCPP_AGENT_SINTENT_H
