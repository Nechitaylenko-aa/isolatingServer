/**
 * @file CRequestPipeline.cpp
 * @brief Единый конвейер запроса: стадии, ворота, монотонный скат (6a).
 *
 * Вся логика намеренно в одном файле — отладка без перемещения по коду.
 */

#include "core/CRequestPipeline.h"

#include <cstddef>
#include <string>
#include <vector>

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"

namespace fcpp
{

namespace
{

/**
 * @brief Запись таблицы маршрутов: Intent -> инструменты.
 */
struct SRoute
{
    EArea m_area;
    ERelation m_relation;
    EProjection m_projection;
    std::vector<EToolName> m_tools;
    bool m_needs_write;
    bool m_needs_exec;
};

/** @brief Детерминированная таблица маршрутов (§30/§31). */
static const std::vector<SRoute>& routes()
{
    static const std::vector<SRoute> table =
    {
        {EArea::Understand, ERelation::Self,       EProjection::Explanation, {EToolName::Outline, EToolName::Source}, false, false},
        {EArea::Understand, ERelation::Self,       EProjection::Signature,   {EToolName::Definition},                  false, false},
        {EArea::Understand, ERelation::Self,       EProjection::Source,      {EToolName::Source},                      false, false},
        {EArea::Understand, ERelation::Callers,    EProjection::Locations,   {EToolName::Callers},                     false, false},
        {EArea::Understand, ERelation::Callees,    EProjection::Locations,   {EToolName::Callees},                     false, false},
        {EArea::Understand, ERelation::References, EProjection::Locations,   {EToolName::References},                  false, false},
        {EArea::Understand, ERelation::Overrides,  EProjection::Locations,   {EToolName::Overrides, EToolName::Dispatch}, false, false},
        {EArea::Understand, ERelation::Includes,   EProjection::Graph,       {EToolName::Includes},                    false, false},
        {EArea::Understand, ERelation::Self,       EProjection::Graph,       {EToolName::Ancestors, EToolName::Derived}, false, false},
        {EArea::Review,     ERelation::Self,       EProjection::Explanation, {EToolName::Outline, EToolName::Source}, false, false},
        {EArea::Diagnose,   ERelation::Self,       EProjection::Explanation, {EToolName::Outline, EToolName::Source}, false, false},
        {EArea::Design,     ERelation::Self,       EProjection::Explanation, {EToolName::Outline},                     false, false},
        {EArea::Change,     ERelation::References, EProjection::Locations,   {EToolName::References, EToolName::Source}, true, false},
        {EArea::Change,     ERelation::Self,       EProjection::Source,      {EToolName::Source, EToolName::References}, true, false},
        {EArea::Verify,     ERelation::Self,       EProjection::Explanation, {EToolName::Outline},                     false, true}
    };
    return table;
}

/**
 * @brief Сформировать исход.
 * @param kind Класс исхода.
 * @param stage Стадия.
 * @param message Сообщение.
 * @return Исход.
 */
SRequestOutcome make_outcome(EOutcomeKind kind, EStage stage, const std::string& message)
{
    SRequestOutcome outcome;
    outcome.m_kind = kind;
    outcome.m_stage = stage;
    outcome.m_message = message;
    return outcome;
}

} // namespace

CRequestPipeline::CRequestPipeline(CProjectManager* project_manager, const CConfigManager* config_manager)
    : m_project_manager(project_manager)
    , m_config_manager(config_manager)
{
}

bool CRequestPipeline::plan(const SIntent& intent, SPlan& out_plan) const
{
    for (const SRoute& route : routes())
    {
        if (route.m_area == intent.m_area
                && route.m_relation == intent.m_relation
                && route.m_projection == intent.m_projection)
        {
            out_plan.m_tools = route.m_tools;
            out_plan.m_requires_write = route.m_needs_write;
            out_plan.m_requires_exec = route.m_needs_exec;
            return true;
        }
    }
    return false;
}

SRequestOutcome CRequestPipeline::validate(const SIntent& intent) const
{
    // Проверка допустимости значений (закрытая модель: недопустимое -> NotUnderstood).
    if (intent.m_area >= EArea::Count
            || intent.m_relation >= ERelation::Count
            || intent.m_projection >= EProjection::Count
            || intent.m_effect >= EEffect::Count)
    {
        return make_outcome(EOutcomeKind::NotUnderstood, EStage::Validated, "intent out of range");
    }

    // Нет маршрута -> не понят.
    SPlan plan_result;
    if (!plan(intent, plan_result))
    {
        return make_outcome(EOutcomeKind::NotUnderstood, EStage::Validated, "no route for intent");
    }
    return make_outcome(EOutcomeKind::Ok, EStage::Validated, "intent valid");
}

SRequestOutcome CRequestPipeline::resolve_subject(const SIntent& intent) const
{
    // Проект/текущий файл — без конкретного имени допустимы.
    if (intent.m_subject.m_selection == ESubjectSelection::CurrentProject
            || intent.m_subject.m_selection == ESubjectSelection::CurrentFile)
    {
        return make_outcome(EOutcomeKind::Ok, EStage::Resolved, "context subject");
    }

    if (intent.m_subject.m_name.empty())
    {
        return make_outcome(EOutcomeKind::NotUnderstood, EStage::Resolved, "subject name is empty");
    }

    // Чистый роутинг без проекта (benchmark уровня A): имя есть -> считаем разрешённым.
    if (m_project_manager == nullptr || !m_project_manager->has_current())
    {
        return make_outcome(EOutcomeKind::Ok, EStage::Resolved, "subject accepted (no index)");
    }

    const std::vector<SSymbol> matches = m_project_manager->find_symbols(intent.m_subject.m_name);
    if (matches.empty())
    {
        return make_outcome(EOutcomeKind::NotUnderstood, EStage::Resolved,
                            "symbol not found: " + intent.m_subject.m_name);
    }
    if (matches.size() == 1)
    {
        return make_outcome(EOutcomeKind::Ok, EStage::Resolved, "subject resolved: " + matches.front().m_qualified_name);
    }

    // Несколько кандидатов -> ровно один уточняющий вопрос с перечислением (§22).
    SRequestOutcome outcome = make_outcome(EOutcomeKind::Clarify, EStage::Resolved, "ambiguous subject");
    for (const SSymbol& symbol : matches)
    {
        outcome.m_options.push_back(symbol.m_qualified_name);
    }
    return outcome;
}

SRequestOutcome CRequestPipeline::check_policy(const SIntent& intent) const
{
    SPlan plan_result;
    if (!plan(intent, plan_result))
    {
        return make_outcome(EOutcomeKind::NotUnderstood, EStage::PolicyChecked, "no route");
    }

    if (m_config_manager == nullptr)
    {
        return make_outcome(EOutcomeKind::Ok, EStage::PolicyChecked, "policy unchecked");
    }

    const SConfig& config = m_config_manager->config();

    if (plan_result.m_requires_write && !config.m_allow_write)
    {
        return make_outcome(EOutcomeKind::Refused, EStage::PolicyChecked, "allow_write is NO");
    }
    if (plan_result.m_requires_exec && !config.m_allow_exec)
    {
        return make_outcome(EOutcomeKind::Refused, EStage::PolicyChecked, "allow_exec is NO");
    }
    if (!config.m_allow_read)
    {
        return make_outcome(EOutcomeKind::Refused, EStage::PolicyChecked, "allow_read is NO");
    }

    return make_outcome(EOutcomeKind::Ok, EStage::PolicyChecked, "policy allowed");
}

SRequestOutcome CRequestPipeline::run_from_intent(const SIntent& intent) const
{
    // Монотонная цепочка: останавливаемся на первом не-Ok исходе.
    SRequestOutcome outcome = validate(intent);
    if (outcome.m_kind != EOutcomeKind::Ok)
    {
        outcome.m_intent = intent;
        return outcome;
    }

    outcome = resolve_subject(intent);
    if (outcome.m_kind != EOutcomeKind::Ok)
    {
        outcome.m_intent = intent;
        return outcome;
    }

    outcome = check_policy(intent);
    if (outcome.m_kind != EOutcomeKind::Ok)
    {
        outcome.m_intent = intent;
        return outcome;
    }

    SPlan plan_result;
    if (!plan(intent, plan_result))
    {
        return make_outcome(EOutcomeKind::NotUnderstood, EStage::Planned, "no plan");
    }

    SRequestOutcome final_outcome;
    final_outcome.m_kind = EOutcomeKind::Ok;
    final_outcome.m_stage = EStage::Planned;
    final_outcome.m_message = "planned";
    final_outcome.m_intent = intent;
    final_outcome.m_plan = plan_result;
    return final_outcome;
}

SRequestOutcome CRequestPipeline::run(const SIntent& intent) const
{
    return run_from_intent(intent);
}

} // namespace fcpp
