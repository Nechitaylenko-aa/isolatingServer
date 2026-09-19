//
// Слой 3. Резолвер: превращает SCollectedAutomationData в Instrument*/Actuator*/SAtomSet*
// внутри CAutomationModel конкретного CSubProject.
//
// ОБНОВЛЕНО (эта сессия, п.4 из плана): добавлен wire_boundary_patterns() — общий,
// не зависящий от E_PROJECT_TYPE проход по графу (automation_layer_context.md,
// automation_layer_timeline_revision.md §5). НЕ виртуальный и не переопределяется
// подтипами — ровно как зафиксировано в context.md ("в базовом классе, не дублируется
// по подтипам"). Возвращает КАНДИДАТОВ, не пишет в CAutomationModel напрямую — вопрос
// "авто в supervisory или на подтверждение человеком" остаётся открытым
// (timeline_revision.md §5.3), поэтому запись в модель — на совести вызывающего bind().
//
// см. automation_layer_context.md, automation_layer_runner_architecture.md,
// automation_layer_timeline_revision.md
//

#ifndef NYM_PROJECT_IINSTALLATIONTEMPLATE_H
#define NYM_PROJECT_IINSTALLATIONTEMPLATE_H


#include "CAutomationAtoms.h"


class CSubProject;

namespace NCore
{
    class CAutomationModel;
    class Instrument;
    class Actuator;
    class CCap;
    class CCell;
    class NComponent;
    class CConductor;
    struct SCollectedAutomationData;
    struct SDeclaredAutomation;

    /**
     * @brief Результат прохода wire_boundary_patterns — всегда кандидат, не готовое
     * решение. signal_owner/signal_role — источник (например ёмкость, SR_LEVEL_HIGH);
     * target_owner/reaction_role — найденный вверх по потоку актуатор-кандидат.
     * @note Не хранит сам SAutomationAtom — атом (condition+action) собирается тем,
     * кто решает подтвердить кандидат (см. открытый вопрос §5.3 timeline_revision.md).
     */
    struct SBoundaryInterlockCandidate
    {
        NComponent*  signal_owner{nullptr};
        SSignalRole  signal_role{};
        NComponent*  target_owner{nullptr};
        SCommandRole reaction_role{};
        bool         activate{true};
        EBoundaryReactionScope scope{BRS_NORMAL};

        /** @brief НОВОЕ: унифицированное "что активирует", вместо расширения candidate
         * новым полем под каждый новый вид триггера (см. обсуждение backwash-alternation
         * в чате — "нам не нужно создавать ВС98, мы просто попеременно её вызываем").
         * wire_boundary_patterns заполняет TRIG_EVENT с одним SCondition (как раньше,
         * просто теперь также в этой форме); backwash-alternation заполняет TRIG_TIME
         * напрямую, минуя signal_owner/signal_role (они не имеют смысла для времени).
         * Потребитель (build_rotation_atoms и т.п.) копирует trigger как есть — ему
         * не нужно знать, чем именно был вызван target_owner. */
        STrigger     trigger{};
    };

    struct SAlternateRoute
    {
        NComponent* owner{nullptr};
        CConductor*       pipe{nullptr};       // порт, ведущий на альтернативный маршрут
    };

    class IInstallationTemplate
    {

    public:
        IInstallationTemplate() = default;
        IInstallationTemplate(const IInstallationTemplate &) = delete;
        IInstallationTemplate(IInstallationTemplate &&) = delete;
        virtual ~IInstallationTemplate() = default;

        //!< владение результатом — на вызывающем (CAutomationModel::bind)
        static IInstallationTemplate* create(const E_PROJECT_TYPE &type);


        virtual NComponent* find_first_boundary_upstream(CCap *from_input_cap, std::vector<CCell*> &visited) = 0;
        virtual NComponent* find_first_boundary_downstream(CCap *from_input_cap, std::vector<CCell*> &visited) = 0;

        virtual float resolve_tor_threshold(ESetpointSource source, const SCondition &cond) const = 0;

        /** @brief заполняет CAutomationModel владельца proj через его приватные
         *  add_instrument/add_actuator/add_atom_set/... (friend class IInstallationTemplate
         *  на CAutomationModel). data — уже собранные хуки слоя 2. */
        virtual bool bind(CSubProject *proj, const SCollectedAutomationData &data) = 0;


        std::vector<SDeclaredAutomation> wire_schedule_atoms(CAutomationModel *model,
                                 const SCollectedAutomationData &data,
                                 uint16_t target_set_id);


        using CComponentPredicate = std::function<bool(NComponent*)>;

        static std::vector<NComponent*> find_components_upstream_matching(
                CCap *from_input_cap, const CComponentPredicate &pred, std::vector<CCell*> &visited);
        static std::vector<NComponent*> find_components_downstream_matching(
                CCap *from_output_cap, const CComponentPredicate &pred, std::vector<CCell*> &visited);

        // std::vector<SAlternateRoute> detect_alternate_routes(CSubProject *proj);

        static Instrument* add_internal_instrument(CAutomationModel*, NComponent*, const SSignalRole&);
        static Actuator*   add_internal_actuator(CAutomationModel*, NComponent*, const SCommandRole&);
        void add_internal_variable(CAutomationModel*, SVariableBehaviorSpec);
        void add_aggregate_signal(CAutomationModel*, NComponent*, const SAggregateSignalSpec&);
        CSubProject*    sub_project() { return m_subproject; }

    protected:
        CSubProject * m_subproject{nullptr};

        /**
         * @brief Общий проход "от сигнала против потока до первого актуатора с нужной
         * ролью, пропуская пассивные компоненты" — automation_layer_timeline_revision.md §5.2.
         * Не хук компонента и не привязан к E_PROJECT_TYPE, живёт здесь одним экземпляром
         * для всех подтипов.
         * @note Результат — ВСЕГДА кандидаты. Ничего не пишет в CAutomationModel сам.
         */
        std::vector<SBoundaryInterlockCandidate> wire_boundary_patterns(CSubProject *proj,
                                                                        const SCollectedAutomationData &data);

        /**
         * @brief Рекурсивный обход против потока от одной входной кепки компонента-
         * источника сигнала. Пропускает компоненты, required_commands() которых не
         * содержит role; останавливается и добавляет в результат на первом компоненте,
         * который её содержит — дальше по этой ветке не идёт. При разветвлении (более
         * одной поставляющей кепки на трубе) идёт по каждой ветке независимо.
         * @param visited защита от циклов в топологии (не было в исходном описании
         * алгоритма — добавлено мной как разумная предосторожность; топология сейчас
         * не гарантированно ацикличная, если это не так — можно убрать).
         */
        std::vector<NComponent*> find_components_upstream_with_command(
                CCap *from_input_cap,
                ECommandRole role,
                std::vector<CCell*> &visited);
        std::vector<NComponent*> find_components_downstream_with_command(
                CCap *from_input_cap,
                ECommandRole role,
                std::vector<CCell*> &visited);

        std::vector<SAlternateRoute> m_alternate_routes;
        bool is_alternate_pipe(CConductor *pipe) const
        {
            return std::any_of(m_alternate_routes.begin(), m_alternate_routes.end(),
                               [pipe](auto &a){ return a.pipe == pipe; });
        }

        static Instrument* add_instrument(CAutomationModel *model, CCap *anchor, const SSignalRole &role);
        static Instrument* add_service_instrument(CAutomationModel *model, const SSignalRole &role,
                                                  std::function<float()> source);
        static Actuator*   add_actuator(CAutomationModel *model, CCap *anchor, const SCommandRole &role);
        static uint16_t    add_atom_set(CAutomationModel *model, uint16_t id, const Tstring &display_name);
        static void        add_atom_to_set(CAutomationModel *model, uint16_t set_id, const SAutomationAtom &atom);
        //!< НОВОЕ: посредник к CAutomationModel::add_transition — нужен теперь, когда
        //!< SAtomSet несёт transitions отдельным списком (см. CAutomationAtoms.h).
        static void        add_transition(CAutomationModel *model, uint16_t set_id, const STransition &transition);
        static void        add_supervisory_atom(CAutomationModel *model, const SAutomationAtom &atom);
        static void        set_initial_active_set(CAutomationModel *model, uint16_t id);

        /**
         * @brief Единая точка входа: перед тем как атом/переход попадёт в модель через
         * add_atom_to_set/add_supervisory_atom/add_transition, гарантирует, что для
         * каждой state-derived роли (SR_PUMP_STATUS и т.п.), встреченной в условиях
         * триггера, уже есть зарегистрированный Instrument (add_internal_instrument).
         * Не хук конкретного проекта/компонента — общий, вызывается из трёх мест ниже
         * автоматически, поэтому ни один генератор атомов (ротация, backwash, будущие
         * сценарии) не может забыть об этом шаге по отдельности.
         * @note Не трогает TRIG_TIME (там нет SCondition) и не трогает SR_MOTO_HOURS —
         * у него отдельный путь регистрации через variable_behaviors()/CServiceInstrument.
         */
        static void ensure_instruments_for_trigger(CAutomationModel *model, const STrigger &trigger);

        // std::vector<SBoundaryInterlockCandidate> wire_boundary_patterns_impl(CCell *root, const SCollectedAutomationData &data);
    };
}

#endif //NYM_PROJECT_IINSTALLATIONTEMPLATE_H
